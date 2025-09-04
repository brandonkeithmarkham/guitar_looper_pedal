# 🎸 Guitar Looper Project

This repository contains the source code and documentation for my **guitar looper pedal** built on the Electrosmith Daisy Seed/POD platform. The project focuses on real-time audio looping, overdubbing, and undo/redo functionality, with audio stored on an SD card for extended loop times.

## 📄 Project Overview
The goal of this project was to design and implement a digital guitar looper with 5+ minutes of looping time, overdub support, and undo/redo capability. The system captures and plays back uncompressed 16-bit audio, while providing hardware control via buttons, knobs, and an OLED display. Emphasis was placed on balancing low-latency audio performance with SD card storage efficiency.

## 🛠 Tools & Methods
- **Platform**: Electrosmith Daisy Seed / Daisy Pod  
- **Libraries**: libDaisy, DaisySP, FatFS, DaisyDuino (for Arduino IDE prototyping)  
- **Features Implemented**:
  - Real-time looping with up to 5 minutes of audio  
  - Overdub with undo/redo limited to overdub layers  
  - Save/load functionality in WAV format on SD card  
  - OLED display for loop status and messages  
  - Hardware I/O buffers designed for guitar impedance levels  

## 📊 Key Outcomes
- Successfully achieved **5+ minutes of recording time** at 16-bit audio quality  
- Implemented **undo/redo functionality** for overdubs without affecting the base recording  
- Verified reliable **SD card read/write performance** using SDMMC and FatFS  
- Designed input/output buffering for guitar compatibility while powering both op-amp and microcontroller from a shared 5V rail  

## 📄 Documentation
The detailed design process, including schematics, code snippets, and block diagrams, can be found here:  
👉 [**Project Summary**](./E2_06_SDD_Poster.pdf)  

Example source files and abstractions are included in this repository for reference and reproducibility.

## 🔖 Notes
- Buttons, knobs, and the OLED display map directly to loop controls for intuitive operation.  
- Undo is restricted to overdubs, ensuring the initial recording remains intact.  
- All audio files are saved in **16-bit WAV** format for maximum compatibility.  

## 📝 Author
Brandon Markham  
Electrical and Computer Engineering Graduate  

## 📫 Contact
[LinkedIn](https://www.linkedin.com/) | [Email](mailto:youremail@example.com)
