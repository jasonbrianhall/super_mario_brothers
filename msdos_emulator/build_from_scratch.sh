#!/bin/bash

# Check if NES ROM file parameter is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <path_to_nes_smb_file>"
    echo "Example: $0 ~/nes/Super\ Mario\ Bros.\ \(JU\)\ \(PRG0\)\ \[\!].nes"
    exit 1
fi

NES_FILE="$1"

# Check if the NES file exists
if [ ! -f "$NES_FILE" ]; then
    echo "Error: NES file '$NES_FILE' not found!"
    exit 1
fi

# Run the converter
pushd ../converter
python main.py smbdis.asm ../msdos/source/ smb_config
popd

# Build and run rom_to_header
pushd source
g++ rom_to_header.cpp -o rom_to_header
./rom_to_header "$NES_FILE" SMBRom smbRomData
popd

# Build the DOS version
./build_dos.sh
