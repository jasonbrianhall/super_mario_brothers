#ifndef KITTY_RENDERER_HPP
#define KITTY_RENDERER_HPP

#include <cstdint>
#include <cstddef>

class KittyRenderer {
public:
    KittyRenderer(int width, int height, int scale = 2);
    ~KittyRenderer();

    void renderFrame(const uint32_t* argbBuffer);

    static bool enableRawMode();
    static void disableRawMode();
    static int  pollKey();
    static bool isKittySupported();

    int scaledWidth()  const { return m_scaledW; }
    int scaledHeight() const { return m_scaledH; }

private:
    void   scaleBuffer(const uint32_t* src);
    void   encodeAndSend();
    size_t encodePNG(const uint8_t* rgb, int w, int h, uint8_t* out, size_t outCap);
    static size_t base64Encode(const uint8_t* src, size_t srcLen, char* dst);

    int    m_srcW, m_srcH, m_scale;
    int    m_scaledW, m_scaledH;

    uint8_t* m_rgb;          // RGB24 scaled frame        (scaledW*scaledH*3)
    uint8_t* m_pngRaw;       // filter-prepended rows     ((scaledW*3+1)*scaledH)
    uint8_t* m_deflateBuf;   // zlib deflate output
    uint8_t* m_pngBuf;       // assembled PNG file
    char*    m_b64;           // base64 of PNG
    char*    m_writeBuf;      // final escape sequence payload

    size_t m_rgbLen;
    size_t m_pngRawLen;
    size_t m_deflateBufLen;
    size_t m_pngBufLen;
    size_t m_b64BufLen;
    size_t m_writeBufLen;

    static bool s_rawMode;
};

#endif // KITTY_RENDERER_HPP
