#include "KittyRenderer.hpp"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <zlib.h>
#ifdef _WIN32
#  include <conio.h>
#  include <io.h>
#else
#  include <unistd.h>
#  include <termios.h>
#  include <sys/select.h>
#endif

// ─── base64 ──────────────────────────────────────────────────────────────────
static const char kB64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t KittyRenderer::base64Encode(const uint8_t* src, size_t len, char* dst)
{
    char* p = dst;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b  = (uint32_t)src[i] << 16;
        if (i + 1 < len) b |= (uint32_t)src[i+1] << 8;
        if (i + 2 < len) b |= (uint32_t)src[i+2];
        *p++ = kB64[(b >> 18) & 0x3F];
        *p++ = kB64[(b >> 12) & 0x3F];
        *p++ = (i + 1 < len) ? kB64[(b >>  6) & 0x3F] : '=';
        *p++ = (i + 2 < len) ? kB64[(b >>  0) & 0x3F] : '=';
    }
    return (size_t)(p - dst);
}

// ─── PNG encode (RGB24, no alpha) ─────────────────────────────────────────────
// We encode as f=100 (PNG) which Felix supports natively and gets compression
// for free via PNG's IDAT deflate — without needing the o=z flag.
//
// Format: minimal single-IDAT PNG, RGB (colour type 2), 8-bit.
// Returns number of bytes written into `out`, or 0 on failure.
size_t KittyRenderer::encodePNG(const uint8_t* rgb, int w, int h, uint8_t* out, size_t outCap)
{
    // Step 1: prepend filter byte 0 (None) to each row → raw deflate input
    // We use m_pngRaw which is pre-allocated to (w*3+1)*h bytes
    uint8_t* raw = m_pngRaw;
    for (int y = 0; y < h; y++) {
        raw[y * (w * 3 + 1)] = 0x00;  // filter type None
        memcpy(raw + y * (w * 3 + 1) + 1, rgb + y * w * 3, (size_t)w * 3);
    }
    size_t rawLen = (size_t)(w * 3 + 1) * h;

    // Step 2: deflate
    uLongf compLen = (uLongf)m_deflateBufLen;
    if (compress2(m_deflateBuf, &compLen, raw, (uLong)rawLen, Z_BEST_SPEED) != Z_OK)
        return 0;

    // Step 3: assemble PNG manually
    // Signature + IHDR(13) + IDAT(compLen) + IEND = fixed overhead
    size_t needed = 8 + 12+13 + 12+(size_t)compLen + 12;
    if (needed > outCap) return 0;

    uint8_t* p = out;

    auto writeU32 = [&](uint32_t v) {
        *p++ = (v >> 24) & 0xFF;
        *p++ = (v >> 16) & 0xFF;
        *p++ = (v >>  8) & 0xFF;
        *p++ = (v      ) & 0xFF;
    };
    auto writeChunk = [&](const char type[4], const uint8_t* data, uint32_t len) {
        writeU32(len);
        uint32_t crc = (uint32_t)crc32(0, (const Bytef*)type, 4);
        if (len) crc = (uint32_t)crc32(crc, data, len);
        memcpy(p, type, 4); p += 4;
        if (len) { memcpy(p, data, len); p += len; }
        writeU32(crc);
    };

    // PNG signature
    const uint8_t sig[] = {137,80,78,71,13,10,26,10};
    memcpy(p, sig, 8); p += 8;

    // IHDR
    uint8_t ihdr[13] = {};
    ihdr[0]=(w>>24)&0xFF; ihdr[1]=(w>>16)&0xFF; ihdr[2]=(w>>8)&0xFF; ihdr[3]=w&0xFF;
    ihdr[4]=(h>>24)&0xFF; ihdr[5]=(h>>16)&0xFF; ihdr[6]=(h>>8)&0xFF; ihdr[7]=h&0xFF;
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 2;   // colour type: RGB (no alpha — smaller than RGBA)
    writeChunk("IHDR", ihdr, 13);

    // IDAT
    writeChunk("IDAT", m_deflateBuf, (uint32_t)compLen);

    // IEND
    writeChunk("IEND", nullptr, 0);

    return (size_t)(p - out);
}

// ─── raw mode & pollKey (cross-platform) ─────────────────────────────────────
bool KittyRenderer::s_rawMode = false;

#ifndef _WIN32
static struct termios s_origTermios;
#endif

bool KittyRenderer::enableRawMode()
{
#ifdef _WIN32
    s_rawMode = true;
    return true;
#else
    if (!isatty(STDIN_FILENO)) return false;
    if (tcgetattr(STDIN_FILENO, &s_origTermios) < 0) return false;
    struct termios raw = s_origTermios;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return false;
    s_rawMode = true;
    return true;
#endif
}

void KittyRenderer::disableRawMode()
{
#ifdef _WIN32
    s_rawMode = false;
#else
    if (s_rawMode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_origTermios);
        s_rawMode = false;
    }
#endif
}

int KittyRenderer::pollKey()
{
#ifdef _WIN32
    if (_kbhit()) return _getch();
    return 0;
#else
    fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) return (int)c;
    }
    return 0;
#endif
}

bool KittyRenderer::isKittySupported()
{
    if (getenv("KITTY_PID"))  return true;
    const char* t  = getenv("TERM");
    const char* tp = getenv("TERM_PROGRAM");
    if (t  && strstr(t,  "kitty"))   return true;
    if (tp && strstr(tp, "kitty"))   return true;
    if (tp && strstr(tp, "WezTerm")) return true;
    return false;
}

// ─── constructor / destructor ─────────────────────────────────────────────────
KittyRenderer::KittyRenderer(int width, int height, int scale)
    : m_srcW(width), m_srcH(height), m_scale(scale),
      m_scaledW(width * scale), m_scaledH(height * scale)
{
    m_rgbLen        = (size_t)m_scaledW * m_scaledH * 3;
    m_pngRawLen     = (size_t)(m_scaledW * 3 + 1) * m_scaledH;
    m_deflateBufLen = compressBound((uLong)m_pngRawLen);
    // PNG overhead: sig(8) + IHDR chunk(25) + IDAT chunk(12+deflate) + IEND(12)
    m_pngBufLen     = 8 + 25 + 12 + m_deflateBufLen + 12 + 64;
    // base64 of the PNG
    m_b64BufLen     = ((m_pngBufLen + 2) / 3) * 4 + 4;
    // write buffer: b64 + chunk headers
    size_t maxChunks = m_b64BufLen / 4096 + 2;
    m_writeBufLen   = m_b64BufLen + maxChunks * 64 + 64;

    m_rgb        = new uint8_t[m_rgbLen];
    m_pngRaw     = new uint8_t[m_pngRawLen];
    m_deflateBuf = new uint8_t[m_deflateBufLen];
    m_pngBuf     = new uint8_t[m_pngBufLen];
    m_b64        = new char   [m_b64BufLen];
    m_writeBuf   = new char   [m_writeBufLen];
}

KittyRenderer::~KittyRenderer()
{
    delete[] m_rgb;
    delete[] m_pngRaw;
    delete[] m_deflateBuf;
    delete[] m_pngBuf;
    delete[] m_b64;
    delete[] m_writeBuf;
}

// ─── pixel scaling: ARGB → RGB24, nearest-neighbour ──────────────────────────
void KittyRenderer::scaleBuffer(const uint32_t* src)
{
    const int s = m_scale;
    uint8_t* dst = m_rgb;
    uint8_t rowBuf[256 * 8 * 3];  // max 256 src pixels * 8x scale * 3 channels

    for (int y = 0; y < m_srcH; ++y) {
        const uint32_t* srcRow = src + y * m_srcW;
        uint8_t* p = rowBuf;
        for (int x = 0; x < m_srcW; ++x) {
            uint32_t argb = srcRow[x];
            uint8_t r = (argb >> 16) & 0xFF;
            uint8_t g = (argb >>  8) & 0xFF;
            uint8_t b = (argb      ) & 0xFF;
            for (int sx = 0; sx < s; ++sx) { *p++ = r; *p++ = g; *p++ = b; }
        }
        size_t rowBytes = (size_t)m_scaledW * 3;
        for (int sy = 0; sy < s; ++sy) {
            memcpy(dst, rowBuf, rowBytes);
            dst += rowBytes;
        }
    }
}

// ─── encode + send ────────────────────────────────────────────────────────────
// Uses f=100 (PNG) — supported by Felix Terminal without needing o=z.
// The PNG IDAT deflate provides the compression transparently.
static const size_t CHUNK = 4096;

void KittyRenderer::encodeAndSend()
{
    // 1. Encode RGB24 → PNG
    size_t pngLen = encodePNG(m_rgb, m_scaledW, m_scaledH, m_pngBuf, m_pngBufLen);
    if (pngLen == 0) return;  // encode failed

    // 2. Base64 encode the PNG bytes
    size_t b64Len = base64Encode(m_pngBuf, pngLen, m_b64);

    // 3. Assemble all escape sequences into write buffer
    char* wp = m_writeBuf;

    // Reset cursor to top-left so each frame overwrites the previous
    memcpy(wp, "\x1b[H", 3); wp += 3;

    bool   first = true;
    size_t pos   = 0;
    while (pos < b64Len) {
        size_t chunkLen = (b64Len - pos < CHUNK) ? (b64Len - pos) : CHUNK;
        bool   last     = (pos + chunkLen >= b64Len);

        if (first) {
            // f=100: PNG format. No o=z needed — compression is inside the PNG.
            wp += snprintf(wp, 128,
                "\x1b_Ga=T,f=100,m=%d;", last ? 0 : 1);
            first = false;
        } else {
            wp += snprintf(wp, 32, "\x1b_Gm=%d;", last ? 0 : 1);
        }
        memcpy(wp, m_b64 + pos, chunkLen); wp += chunkLen;
        memcpy(wp, "\x1b\\", 2);           wp += 2;
        pos += chunkLen;
    }

    // 4. Single write call (cross-platform via fwrite)
    size_t total = (size_t)(wp - m_writeBuf);
    fwrite(m_writeBuf, 1, total, stdout);
    fflush(stdout);
}

void KittyRenderer::renderFrame(const uint32_t* argbBuffer)
{
    scaleBuffer(argbBuffer);
    encodeAndSend();
}
