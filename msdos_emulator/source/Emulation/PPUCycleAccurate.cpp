#include "PPUCycleAccurate.hpp"
#include "../SMB/SMBEmulator.hpp"

// NES palette
static const uint32_t nesPalette[64] = {
    0x7c7c7c, 0x0000fc, 0x0000bc, 0x4428bc, 0x940084, 0xa80020, 0xa81000, 0x881400,
    0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
    0xbcbcbc, 0x0078f8, 0x0058f8, 0x6844fc, 0xd800cc, 0xe40058, 0xf83800, 0xe45c10,
    0xac7c00, 0x00b800, 0x00a800, 0x00a844, 0x008888, 0x000000, 0x000000, 0x000000,
    0xf8f8f8, 0x3cbcfc, 0x6888fc, 0x9878f8, 0xf878f8, 0xf85898, 0xf87858, 0xfca044,
    0xf8b800, 0xb8f818, 0x58d854, 0x58f898, 0x00e8d8, 0x787878, 0x000000, 0x000000,
    0xfcfcfc, 0xa4e4fc, 0xb8b8f8, 0xd8b8f8, 0xf8b8f8, 0xf8a4c0, 0xf0d0b0, 0xfce0a8,
    0xf8d878, 0xd8f878, 0xb8f8b8, 0xb8f8d8, 0x00fcfc, 0xf8d8f8, 0x000000, 0x000000
};

// bitmap8x8 implementation
void bitmap8x8::create(uint8_t* patternLow, uint8_t* patternHigh) {
    memset(s, 0, 8*8);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (patternLow[y] & (0x80 >> x)) s[y][x] |= 1;
            if (patternHigh[y] & (0x80 >> x)) s[y][x] |= 2;
        }
    }
}

void PPUCycleAccurate::setVBlankFlag(bool flag)
{
    if (flag) {
        ppuStatus |= 0x80;  // Set VBlank flag
    } else {
        ppuStatus &= 0x7F;  // Clear VBlank flag
    }
}

void bitmap8x8::draw_tile(uint16_t* dest, int x, int y, uint8_t palette, uint8_t* paletteRAM) {
    if (x < 0 || y < 0 || x > 256-8 || y > 240-8) {
        return;
    }
    
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int screenX = x + px;
            int screenY = y + py;
            
            if (screenX >= 256 || screenY >= 240) continue;
            
            uint8_t pixel = s[py][px];
            uint8_t colorIndex;
            
            if (pixel == 0) {
                colorIndex = paletteRAM[0];
            } else {
                colorIndex = paletteRAM[palette * 4 + pixel];
            }
            
            uint16_t color = convertToRGB565(colorIndex);
            dest[screenY * 256 + screenX] = color;
        }
    }
}

void bitmap8x8::draw_sprite(uint16_t* dest, int x, int y, uint8_t attributes, uint8_t* spritePalette) {
    if (x < -7 || y < -7 || x >= 256 || y >= 240) return;
    
    bool flipX = attributes & 0x40;
    bool flipY = attributes & 0x80;
    bool priority = attributes & 0x20;
    uint8_t palette = (attributes & 0x03) * 4;
    
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            int screenX = x + (flipX ? 7-px : px);
            int screenY = y + (flipY ? 7-py : py);
            
            if (screenX < 0 || screenY < 0 || screenX >= 256 || screenY >= 240) continue;
            
            uint8_t pixel = s[py][px];
            if (pixel != 0) {
                uint8_t colorIndex = spritePalette[palette + pixel];
                dest[screenY * 256 + screenX] = convertToRGB565(colorIndex);
            }
        }
    }
}

uint16_t bitmap8x8::convertToRGB565(uint8_t colorIndex) {
    if (colorIndex >= 64) colorIndex = 0;
    uint32_t rgb = nesPalette[colorIndex];
    return ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
}

// pattern1k implementation
void pattern1k::create(SMBEmulator& engine, int bankIndex) {
    uint8_t* chrData = engine.getCHR();
    if (!chrData) return;
    
    for (int i = 0; i < 64; i++) {
        if (updated[i]) {
            uint16_t patternAddr = bankIndex * 0x400 + i * 16;
            uint8_t patternLow[8], patternHigh[8];
            
            for (int j = 0; j < 8; j++) {
                if (patternAddr + j + 8 >= 8192) {
                    patternLow[j] = 0;
                    patternHigh[j] = 0;
                } else {
                    patternLow[j] = chrData[patternAddr + j];
                    patternHigh[j] = chrData[patternAddr + j + 8];
                }
            }
            
            p[i].create(patternLow, patternHigh);
            updated[i] = false;
        }
    }
}

void pattern1k::markAllUpdated() {
    memset(updated, true, 64);
}

void pattern1k::markUpdated(int patternIndex) {
    if (patternIndex >= 0 && patternIndex < 64) {
        updated[patternIndex] = true;
    }
}

// patterntable implementation
void patterntable::setbank(int banknum, int pnum, pattern1k* pattern1kArray) {
    if (banknum < 0 || banknum >= 4) return;
    bank[banknum] = pnum;
    pbank[banknum] = &pattern1kArray[pnum];
}

bitmap8x8& patterntable::operator[](uint8_t idx) {
    int bankIndex = idx / 64;
    int patternIndex = idx & 63;
    return pbank[bankIndex]->p[patternIndex];
}

bool patterntable::operator==(const patterntable& other) const {
    return memcmp(bank, other.bank, 4) == 0;
}

// natablecache implementation
natablecache::natablecache() {
    surface = new uint16_t[256 * 240];
    memset(nametable, 0, sizeof(nametable));
    memset(attribute, 0, sizeof(attribute));
    totalupdate();
}

natablecache::~natablecache() {
    delete[] surface;
}

void natablecache::totalupdate() {
    memset(updated, true, sizeof(updated));
    memset(lineupdated, true, sizeof(lineupdated));
}

void natablecache::write(uint16_t addr, uint8_t value) {
    if (addr < 32 * 30) {
        if (nametable[addr] == value) return;
        nametable[addr] = value;
        
        int x = addr % 32;
        int y = addr / 32;
        lineupdated[y] = true;
        updated[y][x] = true;
    } else {
        addr -= 32 * 30;
        if (addr >= 64) return;
        if (attribute[addr] == value) return;
        
        attribute[addr] = value;
        
        int attrX = (addr % 8) * 4;
        int attrY = (addr / 8) * 4;
        
        for (int y = attrY; y < attrY + 4 && y < 30; y++) {
            lineupdated[y] = true;
            for (int x = attrX; x < attrX + 4 && x < 32; x++) {
                updated[y][x] = true;
            }
        }
    }
}

uint8_t natablecache::read(uint16_t addr) {
    if (addr < 32 * 30) {
        return nametable[addr];
    } else {
        addr -= 32 * 30;
        return (addr < 64) ? attribute[addr] : 0;
    }
}

void natablecache::setpatterntable(const patterntable& newpt) {
    if (!(pt == newpt)) {
        pt = newpt;
        totalupdate();
    }
}

void natablecache::refresh(int startY, int endY, uint8_t* paletteRAM) {
    if (startY < 0) startY = 0;
    if (endY > 30) endY = 30;
    
    for (int y = startY; y < endY; y++) {
        if (lineupdated[y]) {
            for (int x = 0; x < 32; x++) {
                if (updated[y][x]) {
                    drawtile(x, y, paletteRAM);
                    updated[y][x] = false;
                }
            }
            lineupdated[y] = false;
        }
    }
}

void natablecache::draw(uint16_t* dest, int sx, int sy, int clipX1, int clipY1, int clipX2, int clipY2, uint8_t* paletteRAM) {
    int tileY1 = (clipY1 - sy) / 8;
    int tileY2 = (clipY2 - sy) / 8 + 1;
    refresh(tileY1, tileY2, paletteRAM);
    
    for (int y = clipY1; y < clipY2 && y < sy + 240; y++) {
        if (y - sy < 0) continue;
        
        for (int x = clipX1; x < clipX2 && x < sx + 256; x++) {
            if (x - sx < 0) continue;
            
            dest[y * 256 + x] = surface[(y - sy) * 256 + (x - sx)];
        }
    }
}

void natablecache::drawtile(int tx, int ty, uint8_t* paletteRAM) {
    if (tx >= 32 || ty >= 30) return;
    
    uint8_t tileIndex = nametable[ty * 32 + tx];
    uint8_t attrByte = attribute[(ty / 4) * 8 + (tx / 4)];
    
    int quadX = (tx / 2) % 2;
    int quadY = (ty / 2) % 2;
    int shift = (quadY * 4) + (quadX * 2);
    uint8_t palette = (attrByte >> shift) & 0x03;
    
    pt[tileIndex].draw_tile(surface, tx * 8, ty * 8, palette, paletteRAM);
}

// PPUCycleAccurate implementation
PPUCycleAccurate::PPUCycleAccurate(SMBEmulator& eng) : engine(eng) {
    numpattern1k = 8;
    pt1k = new pattern1k[numpattern1k];
    memset(pt1kupdated, false, sizeof(pt1kupdated));
    
    for (int i = 0; i < 4; i++) {
        ntc[i] = new natablecache();
    }
    frameScrollX = 0;
    frameCtrl = 0;
    ppuScrollX = 0;
    setMirroring(0);
    reset();
    sprite0Hit=false;
}

void PPUCycleAccurate::setSprite0Hit(bool hit) {
    sprite0Hit = hit;
}

PPUCycleAccurate::~PPUCycleAccurate() {
    delete[] pt1k;
    for (int i = 0; i < 4; i++) {
        delete ntc[i];
    }
}

void PPUCycleAccurate::reset() {
    ppuCtrl = 0;
    ppuMask = 0;
    ppuStatus = 0x80;
    scrollX = scrollY = 0;
    
    memset(paletteRAM, 0, sizeof(paletteRAM));
    memset(oam, 0, sizeof(oam));
    
    bgPatternTable.setbank(0, 0, pt1k);
    bgPatternTable.setbank(1, 1, pt1k);
    bgPatternTable.setbank(2, 2, pt1k);
    bgPatternTable.setbank(3, 3, pt1k);
    
    spritePatternTable = bgPatternTable;
    
    updatePatternTables();
}

void PPUCycleAccurate::setMirroring(int type) {
    mirroring = type;
}

void PPUCycleAccurate::updatePatternTables() {
    uint8_t* chrData = engine.getCHR();
    if (!chrData) return;
    
    static bool firstTime = true;
    if (firstTime) {
        for (int i = 0; i < numpattern1k && i < 8; i++) {
            pt1kupdated[i] = true;
            pt1k[i].markAllUpdated();
        }
        firstTime = false;
    }
    
    for (int i = 0; i < numpattern1k && i < 8; i++) {
        if (pt1kupdated[i]) {
            pt1k[i].create(engine, i);
            pt1kupdated[i] = false;
        }
    }
    
    for (int i = 0; i < 4; i++) {
        ntc[i]->setpatterntable(bgPatternTable);
    }
}

void PPUCycleAccurate::writeAddressRegister(uint8_t value)
{
    if (!writeToggle)
    {
        // Upper byte
        currentAddress = (currentAddress & 0xff) | (((uint16_t)value << 8) & 0xff00);
    }
    else
    {
        // Lower byte
        currentAddress = (currentAddress & 0xff00) | (uint16_t)value;
    }
    writeToggle = !writeToggle;
}

void PPUCycleAccurate::writeDataRegister(uint8_t value)
{
    static bool debugPalette = true;
    
    writeByte(currentAddress, value);
    if (!(ppuCtrl & (1 << 2)))
    {
        currentAddress++;
    }
    else
    {
        currentAddress += 32;
    }
}

void PPUCycleAccurate::writeByte(uint16_t address, uint8_t value)
{
    // Mirror all addresses above $3fff
    address &= 0x3fff;

    if (address < 0x2000)
    {
        // CHR-RAM write
        engine.writeCHRData(address, value);
        // Mark CHR pattern updated for cache invalidation
        markCHRUpdated(address);
    }
    else if (address < 0x3f00)
    {
        // Nametable write
        uint16_t nametableAddr = getNametableIndex(address);
        
        // Determine which nametable cache to update based on mirroring
        int nametableIndex = getNametableCacheIndex(address);
        if (nametableIndex >= 0 && nametableIndex < 4) {
            // Convert to local address within the nametable
            uint16_t localAddr = nametableAddr % 0x400;
            ntc[nametableIndex]->write(localAddr, value);
        }
    }
    else if (address < 0x3f20)
    {
        // Palette data
        uint8_t paletteIndex = address - 0x3f00;
        
        // Handle palette mirroring
        if (paletteIndex >= 16) {
            paletteIndex = 16 + ((paletteIndex - 16) % 4);
        }
        
        paletteRAM[paletteIndex] = value;

        // Handle special mirroring cases for universal background color
        if (address == 0x3f10 || address == 0x3f14 || address == 0x3f18 || address == 0x3f1c)
        {
            paletteRAM[address - 0x3f10] = value;
        }
        
        // Mark all nametable caches for update when palette changes
        for (int i = 0; i < 4; i++) {
            ntc[i]->totalupdate();
        }
    }
}

// Helper function to get nametable index with mirroring
uint16_t PPUCycleAccurate::getNametableIndex(uint16_t address)
{
    // Nametable mirroring lookup table
    static const uint8_t nametableMirrorLookup[][4] = {
        {0, 0, 1, 1}, // Horizontal mirroring
        {0, 1, 0, 1}  // Vertical mirroring
    };
    
    address = (address - 0x2000) % 0x1000;
    int table = address / 0x400;
    int offset = address % 0x400;
    
    return (nametableMirrorLookup[mirroring][table] * 0x400 + offset) % 2048;
}

// Helper function to determine which nametable cache to use
int PPUCycleAccurate::getNametableCacheIndex(uint16_t address)
{
    static const uint8_t nametableMirrorLookup[][4] = {
        {0, 0, 1, 1}, // Horizontal mirroring
        {0, 1, 0, 1}  // Vertical mirroring
    };
    
    address = (address - 0x2000) % 0x1000;
    int table = address / 0x400;
    
    return nametableMirrorLookup[mirroring][table];
}

void PPUCycleAccurate::writeRegister(uint16_t address, uint8_t value)
{
    static bool debugPPU = true; // Set to false to disable
    
    switch(address)
    {
    // PPUCTRL
    case 0x2000:
        ppuCtrl = value;
        // Cache for next frame's rendering
        cachedCtrl = value;
        break;
        
    // PPUMASK
    case 0x2001:
        ppuMask = value;
        break;
        
    // OAMADDR
    case 0x2003:
        oamAddress = value;
        break;
        
    // OAMDATA
    case 0x2004:
        oam[oamAddress] = value;
        oamAddress++;
        break;
        
    // PPUSCROLL
    case 0x2005:
    if (!writeToggle)
    {
        // SMB alternates between $00 (status bar) and $C6 (game area)
        // We want to keep the non-zero value as the "real" scroll
        if (value != 0) {
            // This is likely the game area scroll value
            if (gameAreaScrollX != value) {
                gameAreaScrollX = value;
            }
        }
        ppuScrollX = value;  // Still update the register for compatibility
    }
    else
    {
        ppuScrollY = value;
    }
    writeToggle = !writeToggle;
    break;
        
    // PPUADDR
    case 0x2006:
        writeAddressRegister(value);
        break;
        
    // PPUDATA
    case 0x2007:
        writeDataRegister(value);
        break;
        
    default:
        break;
    }
}

uint8_t PPUCycleAccurate::readRegister(uint16_t addr) {
    switch (addr) {
        case 0x2002: return ppuStatus;
        case 0x2007: return 0;
        default: return 0;
    }
}

void PPUCycleAccurate::markCHRUpdated(uint16_t addr) {
    int bankIndex = addr / 0x400;
    int patternIndex = (addr % 0x400) / 16;
    
    if (bankIndex < numpattern1k) {
        pt1k[bankIndex].markUpdated(patternIndex);
        pt1kupdated[bankIndex] = true;
    }
}

void PPUCycleAccurate::render(uint16_t* frameBuffer) {
    updatePatternTables();
    
    uint16_t bgColor = convertToRGB565(paletteRAM[0]);
    for (int i = 0; i < 256 * 240; i++) {
        frameBuffer[i] = bgColor;
    }
    
    if (!(ppuMask & 0x08) && !(ppuMask & 0x10)) {
        return;
    }
    
    if (ppuMask & 0x08) {
        drawBackground(frameBuffer);
    }
    
    if (ppuMask & 0x10) {
        drawSprites(frameBuffer);
    }
}

uint16_t PPUCycleAccurate::convertToRGB565(uint8_t colorIndex) {
    if (colorIndex >= 64) colorIndex = 0;
    uint32_t rgb = nesPalette[colorIndex];
    return ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
}

uint32_t PPUCycleAccurate::convertToRGB32(uint8_t colorIndex) {
    if (colorIndex >= 64) colorIndex = 0;
    return nesPalette[colorIndex];
}

void PPUCycleAccurate::drawTileLikeWorkingPPU(uint16_t* frameBuffer, uint16_t tile, int xOffset, int yOffset, uint8_t attribute) {
    for (int row = 0; row < 8; row++) {
        uint8_t* chrData = engine.getCHR();
        uint8_t plane1 = chrData[tile * 16 + row];
        uint8_t plane2 = chrData[tile * 16 + row + 8];

        for (int column = 0; column < 8; column++) {
            uint8_t paletteIndex = (((plane1 & (1 << column)) ? 1 : 0) + 
                                   ((plane2 & (1 << column)) ? 2 : 0));
            
            uint8_t colorIndex;
            if (paletteIndex == 0) {
                colorIndex = paletteRAM[0];
            } else {
                colorIndex = paletteRAM[(attribute & 0x03) * 4 + paletteIndex];
            }
            
            uint32_t pixel32 = convertToRGB32(colorIndex);
            uint16_t pixel16 = ((pixel32 & 0xF80000) >> 8) | 
                              ((pixel32 & 0x00FC00) >> 5) | 
                              ((pixel32 & 0x0000F8) >> 3);

            int x = (xOffset + (7 - column));
            int y = (yOffset + row);
            if (x >= 0 && x < 256 && y >= 0 && y < 240) {
                frameBuffer[y * 256 + x] = pixel16;
            }
        }
    }
}

void PPUCycleAccurate::drawBackground(uint16_t* frameBuffer) {
    for (int tileY = 0; tileY < 30; tileY++) {
        for (int tileX = 0; tileX < 32; tileX++) {
            uint8_t tileIndex = ntc[0]->read(tileY * 32 + tileX);
            uint16_t tile = tileIndex + (ppuCtrl & (1 << 4) ? 256 : 0);
            
            uint8_t attrByte = ntc[0]->read(32 * 30 + (tileY / 4) * 8 + (tileX / 4));
            int quadX = (tileX / 2) % 2;
            int quadY = (tileY / 2) % 2;
            int shift = (quadY * 4) + (quadX * 2);
            uint8_t palette = (attrByte >> shift) & 0x03;
            
            drawTileLikeWorkingPPU(frameBuffer, tile, tileX * 8, tileY * 8, palette);
        }
    }
}

void PPUCycleAccurate::drawSprites(uint16_t* frameBuffer) {
    bool sprite8x16 = ppuCtrl & 0x20;
    
    for (int i = 63; i >= 0; i--) {
        uint8_t spriteY = oam[i * 4];
        uint8_t tileIndex = oam[i * 4 + 1];
        uint8_t attributes = oam[i * 4 + 2];
        uint8_t spriteX = oam[i * 4 + 3];
        
        if (spriteY >= 240) continue;
        
        if (sprite8x16) {
            spritePatternTable[tileIndex & 0xFE].draw_sprite(frameBuffer, spriteX, spriteY, attributes, &paletteRAM[16]);
            spritePatternTable[tileIndex | 0x01].draw_sprite(frameBuffer, spriteX, spriteY + 8, attributes, &paletteRAM[16]);
        } else {
            spritePatternTable[tileIndex].draw_sprite(frameBuffer, spriteX, spriteY, attributes, &paletteRAM[16]);
        }
    }
}

void PPUCycleAccurate::captureFrameScroll() {
    // Capture the current scroll value AND control register at the start of VBlank
    frameScrollX = ppuScrollX;
    frameCtrl = ppuCtrl;
}  

void PPUCycleAccurate::stepCycle(int scanline, int cycle) {
    // No-op for frame-based rendering
}

void PPUCycleAccurate::getFrameBuffer(uint16_t* buffer) {
    render(buffer);
}
