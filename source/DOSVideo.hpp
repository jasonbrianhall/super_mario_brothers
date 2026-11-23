/**
 * @file
 * @brief Screen output for the MS-DOS build (DJGPP + SDL3's DOS port).
 */
#ifndef DOSVIDEO_HPP
#define DOSVIDEO_HPP

#ifdef __DJGPP__

#include <cstdint>
#include <SDL3/SDL.h>

/**
 * Pick a video mode (SMB_VIDEO=WxHxBITS, or the best one available) and make
 * the full-screen window in it.
 */
SDL_Window* dosCreateWindow(const char* title);

/**
 * Put a 256x240 ARGB8888 frame on the screen.
 */
void dosPresent(SDL_Window* window, const uint32_t* frame);

#endif // __DJGPP__

#endif // DOSVIDEO_HPP
