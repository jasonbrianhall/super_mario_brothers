/*
 * Screen output for the MS-DOS build (DJGPP + SDL3's DOS port).
 *
 * The other front ends draw the 256x240 NES picture with an SDL renderer
 * (scaled by the GPU). On DOS there is only the software renderer, so here
 * the frame goes straight onto the window surface:
 *
 *   hicolor VESA mode (15/16/24/32 bit): the frame is converted and scaled by
 *     a whole number (640x480 -> doubled, 512x480 of it used), centered.
 *   8-bit VESA mode (e.g. 640x480x8, 320x240x8): the NES only has 64
 *     colors, so the picture is exact with a fixed palette (the NES palette,
 *     or the custom one from smbc.conf), and there is half as much to copy
 *     as in hicolor.
 *   VGA mode 13h (320x200, 256 colors): as above, the 8 overscan lines at
 *     the top and bottom are dropped and the remaining 224 are squeezed
 *     into 200.
 *
 * Fast path: every pixel the PPU draws comes from the 64-entry paletteRGB
 * table, so (unless a smoothing filter is on) that table is replaced by one
 * holding each color already in the screen's pixel format -- the palette
 * index in an 8-bit mode, the RGB565 value in a 16-bit one -- and putting
 * the frame on the screen is a plain narrowing copy, no conversion.
 *
 * Which mode: the SMB_VIDEO environment variable (e.g. SET SMB_VIDEO=
 * 640x480x8, or 320x200x8 for mode 13h), otherwise the smallest hicolor mode
 * the picture fits in twice (640x480), otherwise one it fits in once,
 * otherwise an 8-bit mode, otherwise mode 13h.
 */
#ifdef __DJGPP__

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Configuration.hpp"
#include "Constants.hpp"
#include "DOSVideo.hpp"
#include "Util/Video.hpp"

static SDL_Rect dosDest;        // where the picture lands on the screen
static int dosScale = 1;        // whole-number scale (0 = squeezed to fit)
static bool dosIndexed = false;
static bool dosFirstFrame = true;
static bool dosDirect = false;     // paletteRGB holds screen pixel values
static const uint32_t* realPalette = nullptr;   // the RGB palette (for the DAC)
static uint32_t screenPalette[256];             // paletteRGB in direct mode

// The 8 lines at the top and bottom of the NES picture are overscan (a TV
// hides them); dropped when the screen has fewer than 240 lines.
#define OVERSCAN 8

// ---------------------------------------------------------------- mode choice
static int bppRank(SDL_PixelFormat f)
{
    switch (SDL_BITSPERPIXEL(f)) {
    case 16: return 4;          // least to copy of the hicolor modes
    case 15: return 3;
    case 32: return 2;
    case 24: return 1;          // 3-byte pixels: slow to convert to
    default: return 0;
    }
}

static bool dosPickMode(SDL_DisplayMode* chosen)
{
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &count);
    if (modes == NULL || count == 0) return false;

    int wantW = 0, wantH = 0, wantBpp = 0;
    const char* env = getenv("SMB_VIDEO");
    if (env != NULL) sscanf(env, "%dx%dx%d", &wantW, &wantH, &wantBpp);

    const SDL_DisplayMode* best = NULL;
    long bestScore = -1;
    for (int i = 0; i < count; ++i) {
        const SDL_DisplayMode* m = modes[i];
        if (m->w < 320 || m->h < 200) continue;
        int bpp = SDL_BITSPERPIXEL(m->format);
        bool indexed = SDL_ISPIXELFORMAT_INDEXED(m->format);
        if (indexed && bpp != 8) continue;
        long area = (long)m->w * m->h;
        long score;
        if (wantW > 0) {
            // As asked, as closely as possible.
            score = 1000000 - labs(area - (long)wantW * wantH) / 16;
            if (m->w == wantW && m->h == wantH) score += 1000000;
            if (wantBpp != 0 && bpp == wantBpp) score += 500000;
            if (wantBpp == 0 && !indexed) score += 1000 + bppRank(m->format);
        } else {
            bool fits2 = m->w >= RENDER_WIDTH * 2 && m->h >= RENDER_HEIGHT * 2;
            bool fits1 = m->w >= RENDER_WIDTH && m->h >= RENDER_HEIGHT;
            // Doubled first (sharp pixels; a monitor stretching 320x240 blurs),
            // then 1:1, then squeezed; hicolor before 8-bit within each; then
            // the smallest screen (least to copy), then the cheapest format.
            int cls = fits2 ? 3 : fits1 ? 2 : 1;
            score = cls * 4000000L + (indexed ? 0 : 2000000L);
            score += 1000000 - area / 16;
            score += bppRank(m->format);
        }
        if (score > bestScore) { bestScore = score; best = m; }
    }
    if (best) *chosen = *best;
    SDL_free(modes);
    return best != NULL;
}

SDL_Window* dosCreateWindow(const char* title)
{
    SDL_DisplayMode mode;
    bool haveMode = dosPickMode(&mode);
    int w = haveMode ? mode.w : 320;
    int h = haveMode ? mode.h : 200;
    SDL_Window* window = SDL_CreateWindow(title, w, h, SDL_WINDOW_FULLSCREEN);
    if (window == NULL) return NULL;
    if (haveMode) SDL_SetWindowFullscreenMode(window, &mode);
    SDL_SyncWindow(window);

    SDL_Surface* screen = SDL_GetWindowSurface(window);
    if (screen == NULL) {
        printf("dosCreateWindow: SDL_GetWindowSurface: %s\n", SDL_GetError());
        return window;
    }
    dosIndexed = SDL_ISPIXELFORMAT_INDEXED(screen->format);
    if (dosIndexed) {
        // The window surface has no palette of its own: share the one of the
        // driver's surface behind it, which is the one that goes to the DAC.
        SDL_Surface* internal = (SDL_Surface*)SDL_GetPointerProperty(
            SDL_GetWindowProperties(window), "SDL.internal.window.surface", NULL);
        SDL_Palette* pal = internal ? SDL_GetSurfacePalette(internal) : NULL;
        if (pal) SDL_SetSurfacePalette(screen, pal);
        else if (!SDL_GetSurfacePalette(screen)) SDL_CreateSurfacePalette(screen);
    }

    // Layout: the biggest whole-number scale that fits, centered; on a
    // screen with fewer than 240 lines (320x200), squeezed vertically.
    dosScale = SDL_min(screen->w / RENDER_WIDTH, screen->h / RENDER_HEIGHT);
    if (dosScale >= 1) {
        dosDest.w = RENDER_WIDTH * dosScale;
        dosDest.h = RENDER_HEIGHT * dosScale;
    } else {
        dosDest.w = RENDER_WIDTH;
        dosDest.h = screen->h;
    }
    dosDest.x = (screen->w - dosDest.w) / 2;
    dosDest.y = (screen->h - dosDest.h) / 2;
    dosFirstFrame = true;

    // Direct path: paletteRGB in the screen's pixel format. The smoothing
    // filters mix the RGB values, so with them on the frame is converted.
    realPalette = paletteRGB;
    SDL_PixelFormat f = screen->format;
    bool filters = Configuration::getHqdn3dEnabled() || Configuration::getAntiAliasingEnabled();
    dosDirect = !filters && (f == SDL_PIXELFORMAT_INDEX8 || f == SDL_PIXELFORMAT_RGB565 ||
                             f == SDL_PIXELFORMAT_XRGB1555 || f == SDL_PIXELFORMAT_XRGB8888 ||
                             f == SDL_PIXELFORMAT_ARGB8888);
    if (dosDirect) {
        for (int i = 0; i < 256; ++i) {
            uint32_t c = realPalette[i & 63];
            switch (f) {
            case SDL_PIXELFORMAT_INDEX8:   screenPalette[i] = i & 63; break;
            case SDL_PIXELFORMAT_RGB565:   screenPalette[i] = ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F); break;
            case SDL_PIXELFORMAT_XRGB1555: screenPalette[i] = ((c >> 9) & 0x7C00) | ((c >> 6) & 0x03E0) | ((c >> 3) & 0x001F); break;
            default:                       screenPalette[i] = c & 0xFFFFFF; break;
            }
        }
        paletteRGB = screenPalette;
    }

    printf("Video: %dx%d, %d bits%s, picture %dx%d\n", screen->w, screen->h,
           SDL_BITSPERPIXEL(screen->format), dosIndexed ? " (palette)" : "",
           dosDest.w, dosDest.h);
    return window;
}

// ---------------------------------------------------------------- 8-bit
// The palette: the 64 NES colors. Anything else (from the smoothing filters)
// maps to the nearest of them; frame colors -> index through a small cache.
#define CACHE_SIZE 1024
static uint32_t cacheKey[CACHE_SIZE];
static uint8_t  cacheVal[CACHE_SIZE];
static int      cacheUsed;
static SDL_Color nesPal[64];

static void setupPalette(SDL_Surface* screen)
{
    SDL_Color pal[256];
    memset(pal, 0, sizeof(pal));
    for (int i = 0; i < 64; ++i) {
        uint32_t c = realPalette[i];
        nesPal[i].r = (c >> 16) & 0xFF;
        nesPal[i].g = (c >> 8) & 0xFF;
        nesPal[i].b = c & 0xFF;
        nesPal[i].a = 255;
        pal[i] = nesPal[i];
    }
    for (int i = 64; i < 256; ++i) pal[i].a = 255;
    SDL_Palette* screenPal = SDL_GetSurfacePalette(screen);
    if (screenPal) SDL_SetPaletteColors(screenPal, pal, 0, 256);

    memset(cacheKey, 0xFF, sizeof(cacheKey));
    cacheUsed = 0;
}

static inline unsigned hashRGB(uint32_t c) { return (c * 2654435761u) >> 22; }

static uint8_t nearestIndex(uint32_t c)
{
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    int best = 0;
    long bestD = 1L << 30;
    for (int i = 0; i < 64; ++i) {
        int dr = r - nesPal[i].r, dg = g - nesPal[i].g, db = b - nesPal[i].b;
        long d = 3L * dr * dr + 4L * dg * dg + 2L * db * db;
        if (d < bestD) { bestD = d; best = i; if (d == 0) break; }
    }
    return (uint8_t)best;
}

static uint8_t lookupIndex(uint32_t c)
{
    c &= 0xFFFFFF;
    unsigned h = hashRGB(c);
    while (cacheKey[h] != 0xFFFFFFFFu) {
        if (cacheKey[h] == c) return cacheVal[h];
        h = (h + 1) & (CACHE_SIZE - 1);
    }
    uint8_t idx = nearestIndex(c);
    if (cacheUsed >= CACHE_SIZE * 3 / 4) {      // many filtered colors: start over
        memset(cacheKey, 0xFF, sizeof(cacheKey));
        cacheUsed = 0;
        h = hashRGB(c);
    }
    cacheKey[h] = c;
    cacheVal[h] = idx;
    ++cacheUsed;
    return idx;
}

// ---------------------------------------------------------------- rows
// One source row -> one screen row, in the screen's format, scaled by sx.
static void convertRow(const uint32_t* src, uint8_t* dst, SDL_PixelFormat f, int sx)
{
    switch (f) {
    case SDL_PIXELFORMAT_INDEX8: {
        uint32_t last = 0xFFFFFFFFu;
        uint8_t v = 0;
        for (int x = 0; x < RENDER_WIDTH; ++x) {
            uint32_t c = src[x];
            if (c != last) { last = c; v = lookupIndex(c); }
            for (int k = 0; k < sx; ++k) *dst++ = v;
        }
    } break;
    case SDL_PIXELFORMAT_RGB565: {
        uint16_t* o = (uint16_t*)dst;
        for (int x = 0; x < RENDER_WIDTH; ++x) {
            uint32_t c = src[x];
            uint16_t v = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
            for (int k = 0; k < sx; ++k) *o++ = v;
        }
    } break;
    case SDL_PIXELFORMAT_XRGB1555: {
        uint16_t* o = (uint16_t*)dst;
        for (int x = 0; x < RENDER_WIDTH; ++x) {
            uint32_t c = src[x];
            uint16_t v = (uint16_t)(((c >> 9) & 0x7C00) | ((c >> 6) & 0x03E0) | ((c >> 3) & 0x001F));
            for (int k = 0; k < sx; ++k) *o++ = v;
        }
    } break;
    case SDL_PIXELFORMAT_XRGB8888:
    case SDL_PIXELFORMAT_ARGB8888: {
        uint32_t* o = (uint32_t*)dst;
        if (sx == 1) { memcpy(o, src, RENDER_WIDTH * 4); break; }
        for (int x = 0; x < RENDER_WIDTH; ++x)
            for (int k = 0; k < sx; ++k) *o++ = src[x];
    } break;
    case SDL_PIXELFORMAT_BGR24:     // B, G, R in memory
        for (int x = 0; x < RENDER_WIDTH; ++x) {
            uint32_t c = src[x];
            for (int k = 0; k < sx; ++k) {
                dst[0] = (uint8_t)c; dst[1] = (uint8_t)(c >> 8); dst[2] = (uint8_t)(c >> 16);
                dst += 3;
            }
        }
        break;
    default:                        // RGB24: R, G, B in memory
        for (int x = 0; x < RENDER_WIDTH; ++x) {
            uint32_t c = src[x];
            for (int k = 0; k < sx; ++k) {
                dst[0] = (uint8_t)(c >> 16); dst[1] = (uint8_t)(c >> 8); dst[2] = (uint8_t)c;
                dst += 3;
            }
        }
        break;
    }
}

// Direct path: the frame's pixels are already screen values (in the low bits).
static void copyRow(const uint32_t* src, uint8_t* dst, int bpp, int sx)
{
    switch (bpp) {
    case 1:
        if (sx == 1) for (int x = 0; x < RENDER_WIDTH; ++x) dst[x] = (uint8_t)src[x];
        else if (sx == 2) {
            uint16_t* o2 = (uint16_t*)dst;     // two pixels in one store
            for (int x = 0; x < RENDER_WIDTH; ++x) o2[x] = (uint16_t)((src[x] & 0xFF) * 0x101u);
        }
        else for (int x = 0; x < RENDER_WIDTH; ++x) for (int k = 0; k < sx; ++k) *dst++ = (uint8_t)src[x];
        break;
    case 2: {
        uint16_t* o = (uint16_t*)dst;
        if (sx == 1) for (int x = 0; x < RENDER_WIDTH; ++x) o[x] = (uint16_t)src[x];
        else if (sx == 2) {
            uint32_t* o2 = (uint32_t*)dst;     // two pixels in one store
            for (int x = 0; x < RENDER_WIDTH; ++x) o2[x] = (src[x] & 0xFFFF) * 0x10001u;
        }
        else for (int x = 0; x < RENDER_WIDTH; ++x) for (int k = 0; k < sx; ++k) *o++ = (uint16_t)src[x];
    } break;
    default: {
        uint32_t* o = (uint32_t*)dst;
        if (sx == 1) memcpy(o, src, RENDER_WIDTH * 4);
        else for (int x = 0; x < RENDER_WIDTH; ++x) for (int k = 0; k < sx; ++k) *o++ = src[x];
    } break;
    }
}

static bool knownFormat(SDL_PixelFormat f)
{
    return f == SDL_PIXELFORMAT_INDEX8 || f == SDL_PIXELFORMAT_RGB565 ||
           f == SDL_PIXELFORMAT_XRGB1555 || f == SDL_PIXELFORMAT_XRGB8888 ||
           f == SDL_PIXELFORMAT_ARGB8888 || f == SDL_PIXELFORMAT_RGB24 ||
           f == SDL_PIXELFORMAT_BGR24;
}

// ---------------------------------------------------------------- present
void dosPresent(SDL_Window* window, const uint32_t* frame)
{
    SDL_Surface* screen = SDL_GetWindowSurface(window);
    if (screen == NULL) return;

    if (dosFirstFrame) {
        if (dosIndexed) setupPalette(screen);
        // Black borders (in 8-bit modes: index 64, past the NES colors).
        SDL_FillSurfaceRect(screen, NULL, dosIndexed ? 64 : 0);
    }

    if (!knownFormat(screen->format)) {
        // Unusual screen format: let SDL convert (slow, but works).
        SDL_Surface* src = SDL_CreateSurfaceFrom(RENDER_WIDTH, RENDER_HEIGHT, SDL_PIXELFORMAT_XRGB8888,
                                                 (void*)frame, RENDER_WIDTH * 4);
        if (src) {
            SDL_BlitSurfaceScaled(src, NULL, screen, &dosDest, SDL_SCALEMODE_NEAREST);
            SDL_DestroySurface(src);
        }
    } else {
        bool mustLock = SDL_MUSTLOCK(screen);
        if (mustLock) SDL_LockSurface(screen);
        int bpp = SDL_BYTESPERPIXEL(screen->format);
        int sx = dosScale >= 1 ? dosScale : 1;
        int lineBytes = RENDER_WIDTH * sx * bpp;
        uint8_t* row = (uint8_t*)screen->pixels + dosDest.y * screen->pitch + dosDest.x * bpp;
        int lastSrcY = -1;
        for (int y = 0; y < dosDest.h; ++y, row += screen->pitch) {
            int srcY = dosScale >= 1
                ? y / dosScale
                : OVERSCAN + y * (RENDER_HEIGHT - 2 * OVERSCAN) / dosDest.h;
            if (srcY == lastSrcY) memcpy(row, row - screen->pitch, lineBytes);
            else if (dosDirect) copyRow(frame + srcY * RENDER_WIDTH, row, bpp, sx);
            else convertRow(frame + srcY * RENDER_WIDTH, row, screen->format, sx);
            lastSrcY = srcY;
        }
        if (mustLock) SDL_UnlockSurface(screen);
    }

    if (dosFirstFrame) {
        SDL_UpdateWindowSurface(window);
        dosFirstFrame = false;
    } else {
        SDL_UpdateWindowSurfaceRects(window, &dosDest, 1);
    }
}

#endif // __DJGPP__
