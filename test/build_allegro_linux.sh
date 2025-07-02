#!/bin/bash
g++ -s allegro4/dos_main.cpp \
                allegro4/Configuration.cpp \
                allegro4/Emulation/APU.cpp \
                allegro4/Emulation/Controller.cpp \
                allegro4/Emulation/MemoryAccess.cpp \
                allegro4/Emulation/PPU.cpp \
                allegro4/SMB/SMB.cpp \
                allegro4/SMB/SMBData.cpp \
                allegro4/SMB/SMBEngine.cpp \
                allegro4/Util/Video.cpp \
                allegro4/Util/VideoFilters.cpp \
                allegro4/SMBRom.cpp \
                -o smb_allegro \
                -O2 -lalleg -lm -fpermissive -w

