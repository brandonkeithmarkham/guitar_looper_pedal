// Guitar Looper – Daisy Pod / libDaisy
// ------------------------------------
// - 5 min mono loop @ 48 kHz (float buffer in SDRAM)
// - Overdub, play/stop, save to WAV/BIN on SD (FatFS)
// - Encoder2 controls dry/wet mix
// - Button1: Play/Pause   |  Button2: Record/Overdub
// - Hold B1+B2 (>=1s): Reset loop
//
// NOTE: Requires your OledManager.h/.cpp (handles OLED + small UI)

#include "daisysp.h"
#include "daisy_pod.h"
#include "OledManager.h"
#include "fatfs.h"
#include "WavWriter.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;
using namespace daisysp;

// -----------------------------------------------------------------------------
// Build-time config
// -----------------------------------------------------------------------------
#define SAMPLE_RATE       48000.0f
#define MAX_SIZE          (48000 * 60 * 5) // 5 minutes of floats @ 48 kHz
#define WAV_TRANSFER_SIZE 8192             // Writer buffer chunk

// -----------------------------------------------------------------------------
// Globals / hardware
// -----------------------------------------------------------------------------
static DaisyPod                       pod;
static WavWriter<WAV_TRANSFER_SIZE>   wav_writer;
static OledManager                    oledManager;

SdmmcHandler   sd;
FatFSInterface fsi;

constexpr Pin LED_PLAY_PIN = seed::D19;
constexpr Pin LED_REC_PIN  = seed::D20;

dsy_gpio play_led, rec_led;

// -----------------------------------------------------------------------------
// Looper state
// -----------------------------------------------------------------------------
bool  first       = true;   // still capturing initial loop length
bool  rec         = false;  // recording/overdubbing
bool  play        = false;  // playback
int   pos         = 0;      // read/write head
int   mod         = MAX_SIZE; // loop length modulo
int   len         = 0;      // provisional length during first take
float DSY_SDRAM_BSS buf[MAX_SIZE];

float drywet      = 0.0f;   // 0 = fully dry, 1 = fully wet
bool  armed_reset = false;  // helper for reset gesture

int   file_counter  = 1;    // WAV index
int   file_counterb = 1;    // BIN index

// Binary save/load temp
FIL       binary_file;
bool      binary_recording = false;
uint32_t  binary_wptr      = 0;
static int16_t binary_buffer[32768];

// -----------------------------------------------------------------------------
// Forward decls
// -----------------------------------------------------------------------------
static void ResetBuffer();
static void UpdateButtons();
static void Controls();
static void SaveBufferToWav();
static void SaveBufferToBinary();
static void LoadBinaryFile(const char* filename);

static void WriteBuffer(AudioHandle::InterleavingInputBuffer in, size_t i);
static void NextSamples(float& output,
                        AudioHandle::InterleavingInputBuffer in,
                        size_t i);
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size);

// -----------------------------------------------------------------------------
// Audio callback
// -----------------------------------------------------------------------------
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    float output = 0.0f;
    for(size_t i = 0; i < size; i += 2)
    {
        NextSamples(output, in, i);
        out[i]     = output; // L
        out[i + 1] = output; // R (mono)
    }
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(void)
{
    pod.Init();
    pod.SetAudioBlockSize(4);

    // LEDs
    play_led.pin  = LED_PLAY_PIN;
    play_led.mode = DSY_GPIO_MODE_OUTPUT_PP;
    dsy_gpio_init(&play_led);

    rec_led.pin   = LED_REC_PIN;
    rec_led.mode  = DSY_GPIO_MODE_OUTPUT_PP;
    dsy_gpio_init(&rec_led);

    // OLED/UI
    oledManager.Init(pod);

    // SD / FatFS
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sd_cfg.width = SdmmcHandler::BusWidth::BITS_1;
    sd_cfg.speed = SdmmcHandler::Speed::SLOW;

    if(sd.Init(sd_cfg) != SdmmcHandler::Result::OK)
    {
        oledManager.ShowMessage("SD init failed", 1500);
        while(1) {}
    }
    if(fsi.Init(FatFSInterface::Config::MEDIA_SD) != FatFSInterface::Result::OK)
    {
        oledManager.ShowMessage("FS init failed", 1500);
        while(1) {}
    }
    if(f_mount(&fsi.GetSDFileSystem(), "/", 1) != FR_OK)
    {
        oledManager.ShowMessage("Mount failed", 1500);
        while(1) {}
    }

    // WAV writer
    WavWriter<WAV_TRANSFER_SIZE>::Config wav_cfg;
    wav_cfg.samplerate    = SAMPLE_RATE;
    wav_cfg.channels      = 1;
    wav_cfg.bitspersample = 16;
    wav_writer.Init(wav_cfg);

    ResetBuffer();

    pod.StartAdc();
    pod.StartAudio(AudioCallback);

    double battery_voltage = 9.0; // placeholder for your battery code

    while(1)
    {
        Controls();

        // Simple on-screen menu hook
        int32_t enc_move  = pod.encoder.Increment();
        bool    enc_press = pod.encoder.RisingEdge();
        oledManager.HandleMenu(enc_move, enc_press);

        // Optional status
        oledManager.UpdateBatteryDisplay(battery_voltage);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Reset loop buffer and signal LEDs
// -----------------------------------------------------------------------------
static void ResetBuffer()
{
    play  = false;
    rec   = false;
    first = true;
    pos   = 0;
    len   = 0;
    mod   = MAX_SIZE;

    for(int i = 0; i < mod; i++) buf[i] = 0.0f;

    // Flash LEDs: alternate REC / PLAY three times
    for(int i = 0; i < 3; i++)
    {
        dsy_gpio_write(&rec_led, 1);
        dsy_gpio_write(&play_led, 0);
        System::Delay(200);

        dsy_gpio_write(&rec_led, 0);
        dsy_gpio_write(&play_led, 1);
        System::Delay(200);
    }
    dsy_gpio_write(&rec_led, 0);
    dsy_gpio_write(&play_led, 0);
}

// -----------------------------------------------------------------------------
// Record (overdub) into buffer
// -----------------------------------------------------------------------------
static void WriteBuffer(AudioHandle::InterleavingInputBuffer in, size_t i)
{
    buf[pos] += in[i];
    if(buf[pos] > 1.0f)  buf[pos] = 1.0f;
    if(buf[pos] < -1.0f) buf[pos] = -1.0f;

    if(first) { len++; }
}

// -----------------------------------------------------------------------------
// Per-sample processing
// -----------------------------------------------------------------------------
static void NextSamples(float& output,
                        AudioHandle::InterleavingInputBuffer in,
                        size_t i)
{
    if(rec) { WriteBuffer(in, i); }

    const float dry_signal  = in[i];
    const float loop_signal = play ? buf[pos] * (drywet * 1.5f) : 0.0f;

    output = dry_signal + loop_signal;

    if(output > 1.0f)  output = 1.0f;
    if(output < -1.0f) output = -1.0f;

    // Finalize first take if we ever ran the buffer full
    if(len >= MAX_SIZE)
    {
        first = false;
        mod   = MAX_SIZE;
        len   = 0;
    }

    if(play)
    {
        pos++;
        pos %= mod;
    }
}

// -----------------------------------------------------------------------------
// Buttons (play/rec/reset)
// -----------------------------------------------------------------------------
static void UpdateButtons()
{
    // Button2: toggle REC/OD; auto-start PLAY on first press
    if(pod.button2.RisingEdge())
    {
        if(first && rec) // finished first take
        {
            first = false;
            mod   = len;
            len   = 0;
        }

        play = true;
        rec  = !rec;

        dsy_gpio_write(&rec_led, rec ? 1 : 0);
        dsy_gpio_write(&play_led, 1);
    }

    // Hold both buttons (>= 1s) to reset loop
    if(pod.button1.TimeHeldMs() >= 1000
       && pod.button2.TimeHeldMs() >= 1000
       && play) // require we were in a session
    {
        ResetBuffer();
    }

    // Button1: Play/Pause (disabled if first && !rec to avoid empty play)
    if(pod.button1.RisingEdge() && !(first && !rec))
    {
        play = !play;
        rec  = false;
        dsy_gpio_write(&rec_led, 0);
        dsy_gpio_write(&play_led, play ? 1 : 0);
    }
}

// -----------------------------------------------------------------------------
// Controls (encoder2 -> dry/wet)
// -----------------------------------------------------------------------------
static void Controls()
{
    pod.ProcessDigitalControls();

    static int32_t enc_accum = 0;
    enc_accum += pod.encoder2.Increment();
    if(enc_accum < 0)   enc_accum = 0;
    if(enc_accum > 100) enc_accum = 100;

    drywet = enc_accum / 100.0f;

    UpdateButtons();
}

// -----------------------------------------------------------------------------
// Save loop to WAV on SD
// -----------------------------------------------------------------------------
static void SaveBufferToWav()
{
    if(file_counter > 10)
    {
        oledManager.ShowMessage("Max files (10)", 1500);
        return;
    }
    if(mod <= 0)
    {
        oledManager.ShowMessage("No data", 1000);
        return;
    }

    char file_name[16];
    snprintf(file_name, sizeof(file_name), "LOOP%d.WAV", file_counter);
    oledManager.ShowMessage("Creating WAV...", 800);

    wav_writer.OpenFile(file_name);
    oledManager.ShowMessage("Writing...", 500);

    for(int i = 0; i < mod; i++)
    {
        float s = buf[i];
        if(i % (WAV_TRANSFER_SIZE / sizeof(float)) == 0)
        {
            wav_writer.Write();
        }
        wav_writer.Sample(&s);

        if(i % 4096 == 0)
        {
            char progress[24];
            int  pct = (i * 100) / mod;
            snprintf(progress, sizeof(progress), "Writing: %d%%", pct);
            oledManager.ShowMessage(progress, 30);
        }
    }
    wav_writer.Write();
    oledManager.ShowMessage("Finalizing...", 800);
    wav_writer.SaveFile();

    char ok[24];
    snprintf(ok, sizeof(ok), "Saved %s", file_name);
    oledManager.ShowMessage(ok, 1500);
    file_counter++;
}

// -----------------------------------------------------------------------------
// Save loop to raw 16-bit PCM (.BIN)
// -----------------------------------------------------------------------------
static void SaveBufferToBinary()
{
    if(file_counterb > 10)
    {
        oledManager.ShowMessage("Max files (10)", 1500);
        return;
    }
    if(mod <= 0)
    {
        oledManager.ShowMessage("No data", 1000);
        return;
    }

    char file_name[16];
    snprintf(file_name, sizeof(file_name), "LOOP%d.BIN", file_counterb);
    oledManager.ShowMessage("Creating BIN...", 800);

    if(f_open(&binary_file, file_name, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        oledManager.ShowMessage("Create failed", 1200);
        return;
    }

    const int CHUNK = 1024;
    for(int i = 0; i < mod; i += CHUNK)
    {
        int n = (i + CHUNK <= mod) ? CHUNK : (mod - i);
        for(int j = 0; j < n; j++)
        {
            float s = buf[i + j];
            if(s > 1.0f)  s = 1.0f;
            if(s < -1.0f) s = -1.0f;
            binary_buffer[j] = (int16_t)(s * 32767.0f);
        }

        UINT written = 0;
        FRESULT r = f_write(&binary_file, binary_buffer, n * sizeof(int16_t), &written);
        if(r != FR_OK)
        {
            f_close(&binary_file);
            oledManager.ShowMessage("Write error", 1200);
            return;
        }

        char progress[24];
        int  pct = ((i + n) * 100) / mod;
        snprintf(progress, sizeof(progress), "Writing: %d%%", pct);
        oledManager.ShowMessage(progress, 30);
    }
    f_sync(&binary_file);
    f_close(&binary_file);

    char ok[24];
    snprintf(ok, sizeof(ok), "Saved %s", file_name);
    oledManager.ShowMessage(ok, 1500);
    file_counterb++;
}

// -----------------------------------------------------------------------------
// Load raw 16-bit PCM (.BIN) into loop buffer
// -----------------------------------------------------------------------------
static void LoadBinaryFile(const char* filename)
{
    FIL     file;
    UINT    bytesRead;
    FRESULT result;

    if(f_open(&file, filename, FA_READ) != FR_OK)
    {
        oledManager.ShowMessage("Open failed", 1200);
        return;
    }

    FSIZE_t file_size = f_size(&file);
    int     sample_count = file_size / (FSIZE_t)sizeof(int16_t);
    if(sample_count > MAX_SIZE)
    {
        sample_count = MAX_SIZE;
        oledManager.ShowMessage("Truncated", 800);
    }

    ResetBuffer();

    static int16_t read_buffer[2048];
    int total_read = 0;

    while(total_read < sample_count)
    {
        int to_read       = (sample_count - total_read);
        if(to_read > 256) to_read = 256;

        result = f_read(&file,
                        read_buffer,
                        to_read * (int)sizeof(int16_t),
                        &bytesRead);
        if(result != FR_OK) { break; }
        if(bytesRead == 0)  { break; }

        int got = bytesRead / (int)sizeof(int16_t);
        for(int i = 0; i < got && (total_read + i) < MAX_SIZE; i++)
        {
            buf[total_read + i] = (float)read_buffer[i] / 32767.0f;
        }
        total_read += got;

        if((total_read % 1024) == 0 || total_read >= (int)(0.9f * sample_count))
        {
            char progress[24];
            int  pct = (total_read * 100) / sample_count;
            snprintf(progress, sizeof(progress), "Load: %d%%", pct);
            oledManager.ShowMessage(progress, 10);
        }
    }
    f_close(&file);

    first = false;
    mod   = total_read;
    len   = 0;

    char msg[32];
    snprintf(msg, sizeof(msg), "Loaded %d smp", total_read);
    oledManager.ShowMessage(msg, 1500);

    if(total_read > 0)
    {
        play = true;
        dsy_gpio_write(&play_led, 1);
    }
}
