#include "OledManager.h"
#include "daisy_pod.h"
#include "fatfs.h"
#include <cstdio>

using namespace daisy;

// Declare external functions (defined in Looper.cpp)
extern void SaveBufferToWav();
extern void LoadWavFile(const char* filename); // NEW: Function to load WAV file into buffer
extern void SaveBufferToBinary();
extern void LoadBinaryFile(const char* filename);


static DaisyPod pod;

void OledManager::Init(daisy::DaisyPod& pod)
{
    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = pod.seed.GetPin(9);
    disp_cfg.driver_config.transport_config.pin_config.reset = pod.seed.GetPin(30);
    display.Init(disp_cfg);
    DrawMenu();
}

void OledManager::ListBinaryFiles()
{
    file_count = 0; // Reset file count
   
    DIR dir;
    FILINFO fno;

    if (f_opendir(&dir, "/") == FR_OK)
    {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] && file_count < max_files)
        {
            // Look for .bin files
            if (strstr(fno.fname, ".bin") || strstr(fno.fname, ".BIN"))
            {
                strncpy(file_list[file_count], fno.fname, sizeof(file_list[file_count]) - 1);
                file_list[file_count][sizeof(file_list[file_count]) - 1] = '\0'; // Ensure null termination
                file_count++;
            }
        }
        f_closedir(&dir);
    }
}

void OledManager::ShowMessage(const char* message, int duration_ms)
{
    display.Fill(false);
    display.SetCursor(0, 20);
    display.WriteString(message, Font_7x10, true);
    display.Update();
    pod.DelayMs(duration_ms);
}

void OledManager::HandleMenu(int32_t inc, bool pressed)
{
    if (!in_submenu)
    {
        if (inc != 0)
        {
            current_menu_index = (current_menu_index + inc + menu_count) % menu_count;
            DrawMenu();
        }
        if (pressed)
        {
            if (current_menu_index == 0) // "Save/Recall" selected
            {
                in_submenu = true;
                current_submenu_index = 0;
                DrawMenu();
            }
        }
    }
    else if (!in_file_selection)
    {
        if (inc != 0)
        {
            current_submenu_index = (current_submenu_index + inc + sub_menu_count) % sub_menu_count;
            DrawMenu();
        }
        if (pressed)
        {
            // Handle the submenu selections
            if (current_submenu_index == 0) // "Save" selected
            {
                ShowMessage("Saving...", 1000);
                SaveBufferToWav();
                SaveBufferToBinary();
                ShowMessage("Save Complete", 1500);
                in_submenu = false;
            }
            else if (current_submenu_index == 1) // "Recall" selected
            {
                ListBinaryFiles();
                if (file_count > 0)
                {
                    in_file_selection = true;
                    selected_file_index = 0;
                }
                else
                {
                    ShowMessage("No Binary Files Found", 1500);
                    in_submenu = false;
                }
            }
            else if (current_submenu_index == 2) // "Exit" selected
            {
                // Exit the submenu without doing anything
                ShowMessage("Exiting Menu", 1000);
                in_submenu = false;
                in_file_selection = false;
            }
            DrawMenu();
        }
    }
    else
    {
        if (file_count > 0)
        {
            if (inc != 0)
            {
                selected_file_index = (selected_file_index + inc + file_count) % file_count;
                DrawMenu();
            }
            if (pressed)
            {
                ShowMessage("Loading...", 1000);
                
                char selectedFile[64];
                snprintf(selectedFile, sizeof(selectedFile), "%s", file_list[selected_file_index]);
                LoadBinaryFile(selectedFile);
                ShowMessage("Loaded!", 1000);
                in_file_selection = false;
                in_submenu = false;
                DrawMenu();
            }
        }
    }
}

void OledManager::DrawMenu()
{
    display.Fill(false);  // Clear display before drawing menu

    if (!in_submenu)
    {
        for (int i = 0; i < menu_count; i++)
        {
            int y_position = 10 + i * 15;
            int text_width = strlen(menu_entries[i]) * 7 + 6;  // Width of text + padding
            
            if (i == current_menu_index)
            {
                // ✅ Only highlight the length of the text
                display.DrawRect(2, y_position - 2, text_width, y_position + 10, true);
                
                // Fill the rectangle manually to make it a full highlight
                for (int x = 3; x < text_width; x++)
                {
                    for (int y = y_position - 1; y < y_position + 9; y++)
                    {
                        display.DrawPixel(x, y, true);
                    }
                }

                display.SetCursor(5, y_position);
                display.WriteString(menu_entries[i], Font_7x10, false);  // Inverted text
            }
            else
            {
                display.SetCursor(5, y_position);
                display.WriteString(menu_entries[i], Font_7x10, true);
            }
        }
    }
    else if (!in_file_selection) // Regular sub-menu
    {
        for (int i = 0; i < sub_menu_count; i++)
        {
            int y_position = 10 + i * 15;
            int text_width = strlen(sub_menu_entries[i]) * 7 + 6;

            if (i == current_submenu_index)
            {
                display.DrawRect(2, y_position - 2, text_width, y_position + 10, true);
                
                for (int x = 3; x < text_width; x++)
                {
                    for (int y = y_position - 1; y < y_position + 9; y++)
                    {
                        display.DrawPixel(x, y, true);
                    }
                }

                display.SetCursor(5, y_position);
                display.WriteString(sub_menu_entries[i], Font_7x10, false);
            }
            else
            {
                display.SetCursor(5, y_position);
                display.WriteString(sub_menu_entries[i], Font_7x10, true);
            }
        }
    }
    else // File selection screen (Smooth Scrolling)
    {
        const int max_visible_files = 3;
        int scroll_start = selected_file_index - max_visible_files / 2;
        if (scroll_start < 0) scroll_start = 0;
        if (scroll_start > file_count - max_visible_files) scroll_start = file_count - max_visible_files;
        if (file_count < max_visible_files) scroll_start = 0;

        for (int i = 0; i < max_visible_files; i++)
        {
            int file_index = scroll_start + i;
            if (file_index >= file_count) break;

            int y_position = 10 + i * 12;
            int text_width = strlen(file_list[file_index]) * 7 + 6;

            if (file_index == selected_file_index)
            {
                display.DrawRect(2, y_position - 2, text_width, y_position + 10, true);
                
                for (int x = 3; x < text_width; x++)
                {
                    for (int y = y_position - 1; y < y_position + 9; y++)
                    {
                        display.DrawPixel(x, y, true);
                    }
                }

                display.SetCursor(5, y_position);
                display.WriteString(file_list[file_index], Font_7x10, false);
            }
            else
            {
                display.SetCursor(5, y_position);
                display.WriteString(file_list[file_index], Font_7x10, true);
            }
        }
    }

    display.Update();
}


void OledManager::UpdateOledStatus(bool play, bool rec)
{
    char status[32];
    if (play && !rec)
        sprintf(status, "Playing...");
    else if (rec)
        sprintf(status, "Recording...");
    else
        sprintf(status, "Stopped...");
    
    display.SetCursor(0, 50);
    display.WriteString("                    ", Font_7x10, true);
    display.SetCursor(0, 50);
    display.WriteString(status, Font_7x10, true);
    display.Update();
}

void OledManager::UpdateBatteryDisplay(double batt_v)
{
    int batt_x = 115;
    int batt_y = 0;
    display.DrawRect(batt_x, batt_y, batt_x + 12, batt_y + 5, true);
    int batt_term_x = batt_x - 2;
    int batt_term_y = batt_y + 1;
    for (int x = batt_term_x; x < batt_term_x + 2; x++)
        for (int y = batt_term_y; y < batt_term_y + 4; y++)
            display.DrawPixel(x, y, true);
    
    int fill_width = (batt_v > 8.25) ? 11 :
                     (batt_v > 7.5)  ? 10 :
                     (batt_v > 6.75) ? 9 :
                     (batt_v > 6.0)  ? 8 :
                     (batt_v > 5.25) ? 7 :
                     (batt_v > 4.5)  ? 6 :
                     (batt_v > 3.75) ? 5 :
                     (batt_v > 3.0)  ? 4 : 3;
    
    for (int x = batt_x + 1; x < batt_x + 1 + fill_width; x++)
        for (int y = batt_y + 1; y < batt_y + 5; y++)
            display.DrawPixel(x, y, true);

    display.Update();
}
