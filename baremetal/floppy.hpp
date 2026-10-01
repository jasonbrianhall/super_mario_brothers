#pragma once
// Polled driver for the PC floppy controller (Intel 82077AA and compatibles)
// on drive A:, using ISA DMA channel 2. IRQ6 stays masked: completion is
// detected by polling the controller.
#include <stdint.h>

bool floppy_init();                                          // reset, find drive A:, read its geometry
bool floppy_read(uint32_t lba, uint32_t count, void* buf);  // 512-byte sectors
bool floppy_write(uint32_t lba, uint32_t count, const void* buf);
// 0: same disk as before, 1: a disk was swapped in, -1: no disk in the drive.
int floppy_check_media();
bool floppy_write_protected();                              // set by the last failed write
void floppy_idle();                                         // done for now: motor off soon
void floppy_poll();                                         // from the main loop: the motor-off timer
