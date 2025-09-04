# Project Name
TARGET = ex_Looper

# Sources
CPP_SOURCES = Looper.cpp OledManager.cpp 

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP
SHYFFT_DIR = ../../stmlib/fft/
USE_FATFS = 1

# Enable Link-Time Optimization (LTO)
CFLAGS += -flto -Os -ffunction-sections -fdata-sections
CXXFLAGS += -flto -Os -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections -flto

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

