#!/bin/sh
# Exercise fat12.cpp on a copy of the boot floppy (or a fresh FAT12 image)
# and check the result with fsck.fat and mtools.
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
if [ -f ../smb-floppy.img ]; then
    cp ../smb-floppy.img "$T/f.img"
else
    mkfs.fat -C -F 12 "$T/f.img" 1440 >/dev/null
    mmd -i "$T/f.img" ::/boot ::/boot/grub
    echo "multiboot /boot/x" > "$T/g"; mcopy -i "$T/f.img" "$T/g" ::/boot/grub/grub.cfg
fi
printf 'made by mtools\n' > "$T/p"; mcopy -i "$T/f.img" "$T/p" ::/PRESET.TXT
printf 'long\n' > "$T/l"; mcopy -i "$T/f.img" "$T/l" "::/preset long name.txt"

${CXX:-g++} -std=gnu++17 -O1 -g -fsanitize=address,undefined -I.. fat12_test.cpp ../fat12.cpp -o "$T/t"
"$T/t" "$T/f.img"

echo "--- fsck.fat"
fsck.fat -n -v "$T/f.img" | tail -3
echo "--- mtools"
mdir -i "$T/f.img" ::/smb
mtype -i "$T/f.img" ::/smb/stats-expert.txt
mtype -i "$T/f.img" ::/smb/history.csv | tail -2
mtype -i "$T/f.img" "::/many/A rather long file name number 39.text"; echo
mdir -i "$T/f.img" ::/many | tail -4
