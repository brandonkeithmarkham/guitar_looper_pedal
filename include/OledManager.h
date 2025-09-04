#ifndef OLED_MANAGER_H
#define OLED_MANAGER_H

#include "daisy_pod.h"
#include "dev/oled_ssd130x.h"

using MyOledDisplay = daisy::OledDisplay<daisy::SSD130x4WireSpi128x64Driver>;

// Declare external function from Looper.cpp
extern void LoadWavFile(const char* filename);

class OledManager
{
  public:
    void Init(daisy::DaisyPod& pod);
    void HandleMenu(int32_t inc, bool pressed);
    void UpdateOledStatus(bool play, bool rec);
    void UpdateBatteryDisplay(double batt_v);
    void ShowMessage(const char* message, int duration_ms = 1000);
    void ListBinaryFiles(); 

  private:
    void DrawMenu();
    void ListWavFiles();
    void LoadSelectedFile(); // NEW: Calls LoadWavFile() when a file is selected

    MyOledDisplay display;

    // Main menu:
    static constexpr int menu_count = 3;
    int current_menu_index = 0;
    const char* menu_entries[menu_count] = {
        "Save/Recall", "Loop/Playback", "Settings"
    };

    // Sub-menu for Save/Recall:
    bool in_submenu = false;
    static constexpr int sub_menu_count = 3;
    int current_submenu_index = 0;
    const char* sub_menu_entries[sub_menu_count] = {
        "Save", "Recall", "Exit"
    };

    // File selection variables
    bool in_file_selection = false;  // NEW: Are we selecting a file?
    static constexpr int max_files = 10; // Limit number of files displayed
    char file_list[max_files][64];       // Store file names
    int file_count = 0;                  // Number of found files
    int selected_file_index = 0;         // Index of selected file
};

#endif // OLED_MANAGER_H
