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
                -I$BUILD_DIR/allegro4-install/include \
                -L$BUILD_DIR/allegro4-install/lib \
                -o $BUILD_DIR/smb.exe \
                -O2 -lalleg -lm -fpermissive -w

