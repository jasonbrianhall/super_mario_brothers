#pragma once
// xHCI USB keyboards (boot protocol). Key reports are translated into PS/2
// set-1 scancodes and queued alongside the PS/2 keyboard's.
// The boot command line option usb=off skips it.
bool usb_init(const char* cmdline);
void usb_poll();   // call once per frame
