// BlocoCore - graficos 2D gerados por codigo (ver art.h)
#include "art.h"
#include "core.h"
#include "font.h"

#define RGB ART_RGB
const uint16_t artPal[2][16] = {
    { RGB(15,23,31), RGB(2,2,3),   RGB(6,20,4),  RGB(10,26,6), RGB(15,10,5), RGB(10,6,3),
      RGB(16,16,17), RGB(10,10,11),RGB(14,9,4),  RGB(23,17,9), RGB(4,15,4),  RGB(28,26,16),
      RGB(22,17,9),  RGB(31,31,31),RGB(29,21,15),RGB(5,11,25) },
    { RGB(15,23,31), RGB(2,2,3),   RGB(24,29,31),RGB(27,20,15),RGB(15,10,5), RGB(10,6,3),
      RGB(16,16,17), RGB(10,10,11),RGB(31,27,6), RGB(24,18,2), RGB(28,3,3),  RGB(5,9,26),
      RGB(12,29,28), RGB(6,28,10), RGB(27,27,28),RGB(20,19,18) },
};

int iconBank(int id) {
    switch (id) {
    case B_GRAVEL: case B_GLASS: case B_COAL_ORE: case B_IRON_ORE: case B_GOLD_ORE:
    case B_REDSTONE_ORE: case B_LAPIS_ORE: case B_DIAMOND_ORE: case B_EMERALD_ORE:
    case B_IRON_BLOCK: case B_GOLD_BLOCK: case B_DIAMOND_BLOCK:
        return 1;
    }
    return id >= I_STICK && id < I_TOOLS_END ? 1 : 0;
}

static uint32_t tileHash(int t, int x, int y) {
    uint32_t h = (uint32_t)t * 2654435761u ^ (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663);
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    return h & 255;
}

static int oreSpot(int x, int y) {
    return ((x - 2) * (x - 2) + (y - 2) * (y - 2) < 3) || ((x - 5) * (x - 5) + (y - 5) * (y - 5) < 3) ||
           (x == 6 && y == 1) || (x == 1 && y == 6);
}

static int blockPixel(int t, int x, int y) {
    uint32_t r = tileHash(t, x, y);
    int edge = x == 0 || y == 0 || x == 7 || y == 7;
    switch (t) {
    case B_GRASS:
        if (y < 2) return r < 90 ? 3 : 2;
        if (y == 2) return r < 128 ? 2 : 4;
        return r < 50 ? 5 : 4;
    case B_DIRT:    return r < 55 ? 5 : 4;
    case B_STONE:   return r < 70 ? 7 : 6;
    case B_COBBLE:  return (y % 3 == 0 || (x + (y / 3) * 3) % 4 == 0) ? 7 : (r < 40 ? 13 : 6);
    case B_SAND:    return r < 30 ? 9 : 11;
    case B_GRAVEL:  return r < 60 ? 7 : (r < 120 ? 15 : (r < 150 ? 5 : 6));
    case B_LOG:     return (x == 1 || x == 4 || x == 6) ? 5 : 8;
    case B_LEAVES:  return r < 25 ? 0 : (r < 80 ? 3 : (r < 110 ? 2 : 10));
    case B_PLANKS:
        if (y == 3 || y == 7) return 8;
        if ((y < 3 && x == 0) || (y > 3 && x == 4)) return 8;
        return r < 25 ? 8 : 9;
    case B_CTABLE:
        if (y < 2) return 9;
        if (y == 2 || x == 0 || x == 7) return 5;
        return ((x == 3 || x == 4) && y > 4) ? 5 : 9;
    case B_FURNACE:
        if (y >= 4 && y <= 6 && x >= 2 && x <= 5) return 1;
        return edge ? 7 : (r < 60 ? 7 : 6);
    case B_GLASS:
        if (edge) return 14;
        return (x == y + 1 || x == y + 2) && x < 6 ? 2 : 0;
    case B_COAL_ORE: case B_IRON_ORE: case B_GOLD_ORE: case B_REDSTONE_ORE:
    case B_LAPIS_ORE: case B_DIAMOND_ORE: case B_EMERALD_ORE: {
        static const uint8_t oc[] = {1, 3, 8, 10, 11, 12, 13};
        if (oreSpot(x, y)) return oc[t - B_COAL_ORE];
        return r < 70 ? 7 : 6;
    }
    case B_IRON_BLOCK:    return edge ? 6 : ((x + y) % 5 == 0 ? 15 : 14);
    case B_GOLD_BLOCK:    return edge ? 9 : ((x + y) % 5 == 0 ? 9 : 8);
    case B_DIAMOND_BLOCK: return edge ? 11 : ((x + y) % 5 == 0 ? 14 : 12);
    case B_BEDROCK:       return r < 80 ? 1 : (r < 170 ? 7 : 6);
    case ART_WALL:        return r < 40 ? 1 : 5;
    case ART_WALL2:       return r < 60 ? 1 : 7;
    }
    return 0;
}

// Arte 8x8: '.' transparente, hex = cor, letras de forma trocadas por cores
static const char *const artLump[8]  = {"........", "..XXX...", ".XXxXX..", ".XXXXxX.", ".xXXXXX.", "..XXxX..", "...XX...", "........"};
static const char *const artIngot[8] = {"........", "........", "...XXXX.", "..XXXXx.", ".XXXXxx.", ".xxxxx..", "........", "........"};
static const char *const artGem[8]   = {"........", "..XXXX..", ".XXXXXX.", ".xXXXXx.", "..xXXx..", "...xx...", "........", "........"};
static const char *const artDust[8]  = {"........", "..X...X.", "....X...", ".X..x.X.", "...X....", ".x...X..", "...X..x.", "........"};
static const char *const artFlint[8] = {"........", "...XX...", "..XXXX..", ".XXxXX..", ".XXXxX..", "..XxX...", "...X....", "........"};
static const char *const artApple[8] = {"....5...", "...5d...", "..aaaa..", ".aaaaaa.", ".aaaaaa.", ".aaaaaa.", "..aaaa..", "........"};
static const char *const artStick[8] = {"........", "......4.", ".....5..", "....4...", "...5....", "..4.....", ".5......", "........"};
static const char *const artPick[8]  = {".HHHHH..", "H..S..H.", "...S....", "...S....", "...S....", "...S....", "...S....", "........"};
static const char *const artAxe[8]   = {"..HHS...", ".HHHS...", ".HH.S...", "....S...", "....S...", "....S...", "....S...", "........"};
static const char *const artShovel[8]= {"...HH...", "..HHHH..", "..HHHH..", "...S....", "...S....", "...S....", "...S....", "........"};
static const char *const artSword[8] = {"......H.", ".....HH.", "....HH..", "...HH...", ".SHH....", "..S.....", ".S.S....", "........"};
static const char *const artPlayer[16] = {
    ".555555.", ".555555.", ".eeeeee.", ".e1ee1e.", ".eeeeee.", "..eeee..", "ffffffff", "efffffff",
    "effffffe", "effffffe", ".ffffff.", ".111111.", ".11..11.", ".11..11.", ".11..11.", ".77..77.",
};
static const char *const artFrame[8] = {"ddd..ddd", "d......d", "d......d", "........", "........", "d......d", "d......d", "ddd..ddd"};
static const char *const artDot[8]   = {"........", "..1111..", ".1dddd1.", ".1dddd1.", ".1dddd1.", ".1dddd1.", "..1111..", "........"};
static const char *const artView[8]  = {"........", "........", "...11...", "..1dd1..", "..1dd1..", "...11...", "........", "........"};

static int artAt(const char *const *art, int x, int y, char a, int ca, char b, int cb) {
    char c = art[y][x];
    if (c == '.') return 0;
    if (a && c == a) return ca;
    if (b && c == b) return cb;
    return c <= '9' ? c - '0' : c - 'a' + 10;
}

int iconPixel(int id, int x, int y) {
    if (id < NUM_BLOCKS || id == ART_WALL || id == ART_WALL2) return blockPixel(id, x, y);
    if (id >= I_TOOLS && id < I_TOOLS_END) {
        static const uint8_t matCol[NUM_MATS] = {3, 7, 14, 8, 12};
        static const char *const *toolArt[4] = {artPick, artAxe, artShovel, artSword};
        return artAt(toolArt[toolType(id)], x, y, 'H', matCol[toolMat(id)], 'S', 4);
    }
    switch (id) {
    case I_STICK:    return artAt(artStick, x, y, 0, 0, 0, 0);
    case I_COAL:     return artAt(artLump, x, y, 'X', 1, 'x', 7);
    case I_CHARCOAL: return artAt(artLump, x, y, 'X', 5, 'x', 1);
    case I_RAW_IRON: return artAt(artLump, x, y, 'X', 3, 'x', 5);
    case I_RAW_GOLD: return artAt(artLump, x, y, 'X', 8, 'x', 9);
    case I_IRON:     return artAt(artIngot, x, y, 'X', 14, 'x', 6);
    case I_GOLD:     return artAt(artIngot, x, y, 'X', 8, 'x', 9);
    case I_DIAMOND:  return artAt(artGem, x, y, 'X', 12, 'x', 11);
    case I_EMERALD:  return artAt(artGem, x, y, 'X', 13, 'x', 7);
    case I_REDSTONE: return artAt(artDust, x, y, 'X', 10, 'x', 5);
    case I_LAPIS:    return artAt(artLump, x, y, 'X', 11, 'x', 1);
    case I_FLINT:    return artAt(artFlint, x, y, 'X', 7, 'x', 1);
    case I_APPLE:    return artAt(artApple, x, y, 0, 0, 0, 0);
    }
    return 0;
}

int extraPixel(int id, int x, int y) {
    switch (id) {
    case ART_PLAYER: return artAt(artPlayer, x, y, 0, 0, 0, 0);
    case ART_FRAME:  return artAt(artFrame, x, y, 0, 0, 0, 0);
    case ART_DOT:    return artAt(artDot, x, y, 0, 0, 0, 0);
    case ART_VIEW:   return artAt(artView, x, y, 0, 0, 0, 0);
    }
    return 0;
}

void artTile(volatile uint32_t *dst, int id) {
    for (int y = 0; y < 8; y++) {
        uint32_t row = 0;
        for (int x = 0; x < 8; x++) row |= (uint32_t)iconPixel(id, x, y) << (x * 4);
        dst[y] = row;
    }
}

void artTile16(volatile uint32_t *dst, int id) {
    for (int t = 0; t < 4; t++)
        for (int y = 0; y < 8; y++) {
            uint32_t row = 0;
            for (int x = 0; x < 8; x++) {
                int sx = ((t & 1) * 8 + x) / 2, sy = ((t >> 1) * 8 + y) / 2;
                row |= (uint32_t)iconPixel(id, sx, sy) << (x * 4);
            }
            dst[t * 8 + y] = row;
        }
}

void artGlyph(volatile uint32_t *dst, int ch, int fg, int shadow, int bg) {
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 95) ch = '?';
    const unsigned char *g = font8[ch - 32];
    for (int y = 0; y < 8; y++) {
        uint32_t row = 0;
        for (int x = 0; x < 8; x++) {
            int on = (g[y] >> x) & 1;
            int sh = !on && shadow && ((x > 0 && (g[y] >> (x - 1) & 1)) || (y > 0 && (g[y - 1] >> x & 1)));
            row |= (uint32_t)(on ? fg : (sh ? shadow : bg)) << (x * 4);
        }
        dst[y] = row;
    }
}
