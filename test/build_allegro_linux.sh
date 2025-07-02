#!/bin/bash
g++ -s source/dos_main.cpp \
                source/Configuration.cpp \
                source/Emulation/APU.cpp \
                source/Emulation/Controller.cpp \
                source/Emulation/MemoryAccess.cpp \
                source/Emulation/PPU.cpp \
                source/SMB/SMB.cpp \
                source/SMB/SMBData.cpp \
                source/SMB/SMBEngine.cpp \
                source/Util/Video.cpp \
                source/Util/VideoFilters.cpp \
                source/SMBRom.cpp \
                -o smb_allegro \
                -O2 -lalleg -lm -fpermissive -w

