// BlocoGBA - clone 2D de blocos (estilo Minecraft visto de lado) para Game Boy Advance
// C puro, sem bibliotecas: registradores do GBA direto. Regras do jogo em blococore.
// Compila com devkitARM (veja o Makefile). Com -DHOST_PREVIEW compila no PC para
// gerar prints usando exatamente o mesmo codigo.
//
// Fase 2: sobrevivencia basica
//  - Mundo 256x64 com cavernas, cascalho e 7 minerios por profundidade
//  - Mineracao com tempo real (formula da wiki), tiers de picareta e drops
//  - Inventario de 36 slots, barra rapida, crafting 2x2/bancada e fornalha
//  - Save em SRAM do mundo + inventario

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "core.h"
#include "art.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;

#ifdef HOST_PREVIEW
#include "host.h"
#else
#define REG_DISPCNT   (*(volatile u16 *)0x04000000)
#define REG_VCOUNT    (*(volatile u16 *)0x04000006)
#define REG_BG0CNT    (*(volatile u16 *)0x04000008)
#define REG_BG1CNT    (*(volatile u16 *)0x0400000A)
#define REG_BG2CNT    (*(volatile u16 *)0x0400000C)
#define REG_BG0HOFS   (*(volatile u16 *)0x04000010)
#define REG_BG0VOFS   (*(volatile u16 *)0x04000012)
#define REG_BG1HOFS   (*(volatile u16 *)0x04000014)
#define REG_BG1VOFS   (*(volatile u16 *)0x04000016)
#define REG_KEYINPUT  (*(volatile u16 *)0x04000130)
#define PAL_BG        ((volatile u16 *)0x05000000)
#define PAL_OBJ       ((volatile u16 *)0x05000200)
#define VRAM          ((volatile u16 *)0x06000000)
#define OBJ_TILES     ((volatile u32 *)0x06010000)
#define OAM           ((volatile u16 *)0x07000000)
#define SRAM          ((volatile u8  *)0x0E000000)
#define EWRAM_BSS     __attribute__((section(".sbss")))
#endif

// Marca para emuladores/flashcards detectarem save em SRAM
const char saveTypeTag[] __attribute__((used, aligned(4))) = "SRAM_V113";


enum {
    K_A = 1, K_B = 2, K_SELECT = 4, K_START = 8, K_RIGHT = 16, K_LEFT = 32,
    K_UP = 64, K_DOWN = 128, K_R = 256, K_L = 512
};

// ---------------- Mundo ----------------
#define W 256
#define H 64
#define T_WALL   ART_WALL
#define T_WALL2  ART_WALL2

static u8 world[H][W] EWRAM_BSS;
static u8 surface[W];

static inline int getBlock(int x, int y) {
    if (y < 0) return B_AIR;
    if (x < 0 || x >= W || y >= H) return B_BEDROCK;
    return world[y][x];
}
static inline bool solid(int x, int y) { return blocksMove(getBlock(x, y)); }   // muda nao conta

// VRAM: tiles de BG no charblock 0; mapas: 28 blocos, 29 paredes, 30 texto/HUD
#define BG_TILES   ((volatile u32 *)VRAM)
#define MAP_FG     (VRAM + 28 * 0x400)
#define MAP_BG     (VRAM + 29 * 0x400)
#define MAP_TXT    (VRAM + 30 * 0x400)
#define FONT_T     64    // glifos transparentes com sombra
#define FONT_P     128   // glifos sobre painel preto
#define OBJT_PLAYER ART_PLAYER
#define OBJT_FRAME  ART_FRAME

static void setupGraphics(void) {
    for (int i = 0; i < 16; i++) {
        PAL_BG[i] = artPal[0][i]; PAL_BG[16 + i] = artPal[1][i];
        PAL_OBJ[i] = artPal[0][i]; PAL_OBJ[16 + i] = artPal[1][i];
    }
    for (int t = 0; t < NUM_BLOCKS; t++) { artTile(BG_TILES + t * 8, t); artTile(OBJ_TILES + t * 8, t); }
    artTile(BG_TILES + T_WALL * 8, T_WALL);
    artTile(BG_TILES + T_WALL2 * 8, T_WALL2);
    for (int id = I_STICK; id <= I_LAST_SIMPLE; id++) artTile(OBJ_TILES + id * 8, id);
    for (int id = I_TOOLS; id < I_TOOLS_END; id++) artTile(OBJ_TILES + id * 8, id);
    for (int g = 0; g < 64; g++) {
        artGlyph(BG_TILES + (FONT_T + g) * 8, 32 + g, 13, 1, 0);   // texto com sombra
        artGlyph(BG_TILES + (FONT_P + g) * 8, 32 + g, 13, 0, 1);   // texto sobre painel
    }
    for (int t = 0; t < 2; t++)
        for (int y = 0; y < 8; y++) {
            u32 row = 0;
            for (int x = 0; x < 8; x++) row |= (u32)extraPixel(ART_PLAYER, x, t * 8 + y) << (x * 4);
            OBJ_TILES[(OBJT_PLAYER + t) * 8 + y] = row;
        }
    for (int y = 0; y < 8; y++) {
        u32 row = 0;
        for (int x = 0; x < 8; x++) row |= (u32)extraPixel(ART_FRAME, x, y) << (x * 4);
        OBJ_TILES[OBJT_FRAME * 8 + y] = row;
    }
    for (int i = 0; i < 32 * 32; i++) MAP_TXT[i] = 0;

    REG_BG0CNT = (1) | (28 << 8);   // blocos: prioridade 1
    REG_BG1CNT = (2) | (29 << 8);   // paredes: prioridade 2
    REG_BG2CNT = (0) | (30 << 8);   // texto: prioridade 0
    REG_DISPCNT = 0x0100 | 0x0200 | 0x0400 | 0x1000 | 0x0040;  // modo 0, BG0-2, OBJ, OBJ 1D
}

// ---------------- Texto (BG2, 30x20 visivel) ----------------
static void txtFill(int r0, int r1, bool panel) {
    for (int r = r0; r <= r1; r++)
        for (int c = 0; c < 32; c++) MAP_TXT[r * 32 + c] = panel ? FONT_P : 0;
}
static void txt(int col, int row, const char *s, bool panel) {
    for (; *s && col < 30; s++, col++) {
        int ch = (unsigned char)*s;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        MAP_TXT[row * 32 + col] = (u16)((panel ? FONT_P : FONT_T) + ch - 32);
    }
}
static void txtNum(int col, int row, int n, int width, bool panel) {
    char b[8];
    int i = 7;
    b[i] = 0;
    do { b[--i] = (char)('0' + n % 10); n /= 10; } while (n && i > 0);
    while (7 - i < width && i > 0) b[--i] = ' ';
    txt(col, row, b + i, panel);
}

// ---------------- Sprites (OAM sombra, copiada no VBlank) ----------------
static u16 oamShadow[128 * 4];
static int nextSprite;

static void sprReset(void) {
    for (int i = 0; i < 128; i++) { oamShadow[i * 4] = 0x0200; oamShadow[i * 4 + 1] = 0; oamShadow[i * 4 + 2] = 0; }
    nextSprite = 0;
}
static void spr(int x, int y, bool tall, int tile, int palb, bool hflip) {
    if (nextSprite >= 128 || x < -16 || x > 240 || y < -16 || y > 160) return;
    int i = nextSprite++;
    oamShadow[i * 4]     = (u16)((y & 255) | (tall ? 0x8000 : 0));
    oamShadow[i * 4 + 1] = (u16)((x & 511) | (hflip ? 0x1000 : 0));
    oamShadow[i * 4 + 2] = (u16)(tile | (palb << 12));
}
static void sprItem(int x, int y, int id) { if (id) spr(x, y, false, id, iconBank(id), false); }

// ---------------- Geracao de terreno ----------------
static u32 seed = 12345;

static int hash2(int x, int y) {
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (int)((h ^ (h >> 16)) & 255);
}
static int noise1(int x, int cell) {
    int g = x / cell, t = (x % cell) * 256 / cell;
    int a = hash2(g, 999), b = hash2(g + 1, 999);
    t = t * t * (768 - 2 * t) >> 16;
    return a + ((b - a) * t >> 8);
}
static int noise2(int x, int y, int cell) {
    int gx = x / cell, gy = y / cell;
    int tx = (x % cell) * 256 / cell, ty = (y % cell) * 256 / cell;
    int a = hash2(gx, gy), b = hash2(gx + 1, gy), c = hash2(gx, gy + 1), d = hash2(gx + 1, gy + 1);
    int ab = a + ((b - a) * tx >> 8), cd = c + ((d - c) * tx >> 8);
    return ab + ((cd - ab) * ty >> 8);
}

static void vein(int ore, int count, int ymin, int ymax, int size, int alsoReplace) {
    for (int i = 0; i < count; i++) {
        int x = (int)(rnd() % W), y = ymin + (int)(rnd() % (u32)(ymax - ymin + 1));
        for (int s = 0; s < size; s++) {
            if (x >= 0 && x < W && y >= 0 && y < H &&
                (world[y][x] == B_STONE || (alsoReplace && world[y][x] == alsoReplace)))
                world[y][x] = (u8)ore;
            switch (rnd() & 3) { case 0: x++; break; case 1: x--; break; case 2: y++; break; default: y--; }
        }
    }
}

static void placeTree(int x, int y) {
    int h = 4 + (int)(rnd() % 3);
    for (int i = 1; i <= h; i++) if (y - i >= 0) world[y - i][x] = B_LOG;
    int top = y - h;
    for (int dy = -2; dy <= 1; dy++)
        for (int dx = -2; dx <= 2; dx++) {
            int lx = x + dx, ly = top + dy;
            if (lx < 0 || lx >= W || ly < 0) continue;
            if (dx * dx + dy * dy > 5 || (dy == -2 && (dx == -2 || dx == 2))) continue;
            if (world[ly][lx] == B_AIR) world[ly][lx] = B_LEAVES;
        }
}

static void generateWorld(void) {
    rndSeed(seed * 2654435761u + 7);
    for (int x = 0; x < W; x++) {
        int h = 20 + (noise1(x, 32) * 10 + noise1(x, 8) * 4) / 255;
        surface[x] = (u8)h;
        for (int y = 0; y < H; y++) {
            int b = B_AIR;
            if (y >= H - 1 || (y >= H - 3 && hash2(x, y) < 128)) b = B_BEDROCK;
            else if (y > h + 3 + (hash2(x, 5) & 1)) b = B_STONE;
            else if (y > h) b = B_DIRT;
            else if (y == h) b = (h >= 32) ? B_SAND : B_GRASS;
            if (b == B_STONE || (b == B_DIRT && y > h + 2)) {
                int n = noise2(x, y * 2, 12) + noise2(x + 500, y * 2, 6) / 2;
                if (n > 245) b = B_AIR;
            }
            world[y][x] = (u8)b;
        }
    }
    // profundidade: carvao no alto, diamante e redstone perto da rocha-matriz
    vein(B_GRAVEL,       26, 24, 56, 12, B_DIRT);
    vein(B_COAL_ORE,     90, 22, 50, 8, 0);
    vein(B_IRON_ORE,     80, 26, 60, 6, 0);
    vein(B_LAPIS_ORE,    18, 40, 58, 5, 0);
    vein(B_GOLD_ORE,     24, 44, 61, 5, 0);
    vein(B_REDSTONE_ORE, 30, 52, 62, 6, 0);
    vein(B_DIAMOND_ORE,  16, 55, 62, 4, 0);
    vein(B_EMERALD_ORE,  30, 24, 36, 1, 0);
    for (int x = 3; x < W - 3; x++)
        if (world[surface[x]][x] == B_GRASS && hash2(x, 3) < 28 &&
            world[surface[x] - 1][x - 1] == B_AIR && world[surface[x] - 1][x - 2] == B_AIR)
            placeTree(x, surface[x]);
}

// ---------------- Mudas ----------------
static int saplingTicks;   // ticks aleatorios por frame (ver core.h)
static bool mapDirty = true;
static bool playerInBox(int x0, int y0, int x1, int y1);

// muda em (x,y) vira carvalho: precisa de terra embaixo, ceu aberto
// (so ar ou folhas acima) e espaco para o tronco mais alto (6)
static bool growSapling(int x, int y) {
    if (!saplingSoil(getBlock(x, y + 1)) || y < 8) return false;
    for (int yy = 0; yy < y; yy++)
        if (world[yy][x] != B_AIR && world[yy][x] != B_LEAVES) return false;
    if (playerInBox(x - 2, y - 8, x + 2, y)) return false;
    world[y][x] = B_AIR;
    world[y + 1][x] = B_DIRT;      // como no Java, a grama embaixo vira terra
    placeTree(x, y + 1);
    mapDirty = true;
    return true;
}

static void randomTicks(void) {
    for (int i = 0; i < saplingTicks; i++) {
        u32 r = rnd() % (W * H);
        int x = (int)(r % W), y = (int)(r / W);
        if (world[y][x] == B_SAPLING && rnd() % SAPLING_GROW_CHANCE == 0) growSapling(x, y);
    }
}

// ---------------- Save em SRAM ----------------
#define SAVE_INV   8
#define SAVE_FUEL  (SAVE_INV + INV_SIZE * 4)
#define SAVE_SURF  (SAVE_FUEL + 2)
#define SAVE_WORLD (SAVE_SURF + W)

static void saveGame(void) {
    SRAM[0] = 'B'; SRAM[1] = 'L'; SRAM[2] = 'K'; SRAM[3] = '2';
    for (int i = 0; i < 4; i++) SRAM[4 + i] = (u8)(seed >> (i * 8));
    for (int i = 0; i < INV_SIZE; i++) {
        SRAM[SAVE_INV + i * 4]     = inv[i].id;
        SRAM[SAVE_INV + i * 4 + 1] = inv[i].count;
        SRAM[SAVE_INV + i * 4 + 2] = (u8)inv[i].dmg;
        SRAM[SAVE_INV + i * 4 + 3] = (u8)(inv[i].dmg >> 8);
    }
    SRAM[SAVE_FUEL] = (u8)fuelHalf; SRAM[SAVE_FUEL + 1] = (u8)(fuelHalf >> 8);
    for (int x = 0; x < W; x++) SRAM[SAVE_SURF + x] = surface[x];
    const u8 *p = &world[0][0];
    for (int i = 0; i < W * H; i++) SRAM[SAVE_WORLD + i] = p[i];
}
static bool loadGame(void) {
    if (SRAM[0] != 'B' || SRAM[1] != 'L' || SRAM[2] != 'K' || SRAM[3] != '2') return false;
    seed = 0;
    for (int i = 0; i < 4; i++) seed |= (u32)SRAM[4 + i] << (i * 8);
    for (int i = 0; i < INV_SIZE; i++) {
        inv[i].id = SRAM[SAVE_INV + i * 4];
        inv[i].count = SRAM[SAVE_INV + i * 4 + 1];
        inv[i].dmg = (u16)(SRAM[SAVE_INV + i * 4 + 2] | (SRAM[SAVE_INV + i * 4 + 3] << 8));
    }
    fuelHalf = SRAM[SAVE_FUEL] | (SRAM[SAVE_FUEL + 1] << 8);
    for (int x = 0; x < W; x++) surface[x] = SRAM[SAVE_SURF + x];
    u8 *p = &world[0][0];
    for (int i = 0; i < W * H; i++) p[i] = SRAM[SAVE_WORLD + i];
    return true;
}

// ---------------- Jogador (ponto fixo 24.8, pixels) ----------------
#define PW 6
#define PH 14
#define GRAVITY   51
#define JUMP_V   740
#define WALK_V   300
#define MAX_FALL 1280

static s32 px, py, vx, vy;
static bool onGround, facingLeft;
static int curDX = 1, curDY = 0;
static int selSlot = 0;
static int camX, camY, lastCTX = -999, lastCTY = -999;
static u16 prevKeys = 0;
static u32 frameCount = 0;

static bool hitboxSolid(int x, int y) {
    for (int ty = y >> 3; ty <= (y + PH - 1) >> 3; ty++)
        for (int tx = x >> 3; tx <= (x + PW - 1) >> 3; tx++)
            if (solid(tx, ty)) return true;
    return false;
}

static bool playerInBox(int x0, int y0, int x1, int y1) {
    int ax = (px >> 8) >> 3, bx = ((px >> 8) + PW - 1) >> 3;
    int ay = (py >> 8) >> 3, by = ((py >> 8) + PH - 1) >> 3;
    return ax <= x1 && bx >= x0 && ay <= y1 && by >= y0;
}

static void spawnPlayer(void) {
    int x = W / 2;
    while (x < W - 1 && world[surface[x]][x] != B_GRASS) x++;
    int y = 0;
    while (y < H - 1 && !solid(x, y)) y++;
    px = (x * 8 + 1) << 8;
    py = (y * 8 - PH) << 8;
    vx = vy = 0;
}

static bool moveAxis(s32 *pos, s32 vel, bool horiz) {
    s32 target = *pos + vel;
    int from = *pos >> 8, to = target >> 8;
    int step = to > from ? 1 : -1;
    while (from != to) {
        int nx = horiz ? from + step : px >> 8;
        int ny = horiz ? py >> 8 : from + step;
        if (hitboxSolid(nx, ny)) { *pos = (s32)from << 8 | (step > 0 ? 255 : 0); return true; }
        from += step;
    }
    *pos = target;
    return false;
}

static void playerCenterTile(int *tx, int *ty) {
    *tx = ((px >> 8) + PW / 2) >> 3;
    *ty = ((py >> 8) + PH / 2) >> 3;
}

// ---------------- Estado de jogo / UI ----------------
static bool menuOpen = false, uiDirty = true;
static int tab = 0, invCur = 0, invPick = -1, craftCur = 0, craftScroll = 0;
static bool hasTable = false, hasFurnace = false;
static const char *toast = NULL;
static int toastTime = 0;
static int mineX = -1, mineY, mineProg, mineNeed, mineCooldown;

static void setToast(const char *t) { toast = t; toastTime = 120; uiDirty = true; }
static void stopMining(void) { if (mineX >= 0) uiDirty = true; mineX = -1; mineProg = 0; }

static bool nearBlock(int type) {
    int cx, cy;
    playerCenterTile(&cx, &cy);
    for (int y = cy - 4; y <= cy + 4; y++)
        for (int x = cx - 4; x <= cx + 4; x++)
            if (x >= 0 && x < W && y >= 0 && y < H && world[y][x] == type) return true;
    return false;
}

static void updatePlayer(u16 keys, u16 down) {
    bool aiming = keys & K_B;
    vx = 0;
    if (!aiming) {
        if (keys & K_LEFT)  { vx = -WALK_V; if (!facingLeft) { facingLeft = true;  if (curDX > 0) curDX = -curDX; } }
        if (keys & K_RIGHT) { vx =  WALK_V; if (facingLeft)  { facingLeft = false; if (curDX < 0) curDX = -curDX; } }
        if ((down & (K_A | K_UP)) && onGround) vy = -JUMP_V;
    } else {
        if ((down & K_LEFT)  && curDX > -4) curDX--;
        if ((down & K_RIGHT) && curDX <  4) curDX++;
        if ((down & K_UP)    && curDY > -4) curDY--;
        if ((down & K_DOWN)  && curDY <  4) curDY++;
    }
    vy += GRAVITY;
    if (vy > MAX_FALL) vy = MAX_FALL;
    moveAxis(&px, vx, true);
    bool hit = moveAxis(&py, vy, false);
    onGround = hit && vy > 0;
    if (hit) vy = 0;
    if ((py >> 8) > H * 8) spawnPlayer();
}

static void aimTile(int *tx, int *ty) {
    playerCenterTile(tx, ty);
    *tx += curDX; *ty += curDY;
}

static void editBlocks(u16 keys, u16 down) {
    int tx, ty;
    aimTile(&tx, &ty);
    bool valid = tx >= 0 && tx < W && ty >= 0 && ty < H;

    // quebrar segurando R (tempo da wiki)
    if (mineCooldown) mineCooldown--;
    if ((keys & K_R) && valid && world[ty][tx] != B_AIR && !mineCooldown) {
        if (mineX != tx || mineY != ty) {
            mineX = tx; mineY = ty; mineProg = 0;
            mineNeed = breakFrames(world[ty][tx], inv[selSlot].id);
        }
        if (mineNeed > 0 && ++mineProg >= mineNeed) {
            harvest(world[ty][tx], selSlot);
            world[ty][tx] = B_AIR;
            if (getBlock(tx, ty - 1) == B_SAPLING) {   // muda sem chao solta
                world[ty - 1][tx] = B_AIR;
                addItem(B_SAPLING, 1);
            }
            mapDirty = true;
            stopMining();
            mineCooldown = 18;    // 6 ticks entre blocos (Java)
        }
        uiDirty = true;
    } else if (mineX >= 0 && (!(keys & K_R) || mineX != tx || mineY != ty)) {
        stopMining();
    }

    // colocar com L
    if ((down & K_L) && valid && world[ty][tx] == B_AIR && inv[selSlot].id == B_SAPLING &&
        !saplingSoil(getBlock(tx, ty + 1))) {
        setToast("MUDA SO EM GRAMA OU TERRA");
    } else if ((down & K_L) && valid && world[ty][tx] == B_AIR && isPlaceable(inv[selSlot].id)) {
        int x0 = px >> 8, y0 = py >> 8;
        bool overlap = tx * 8 < x0 + PW && tx * 8 + 8 > x0 && ty * 8 < y0 + PH && ty * 8 + 8 > y0;
        if (!overlap) {
            world[ty][tx] = inv[selSlot].id;
            if (!--inv[selSlot].count) inv[selSlot].id = 0;
            mapDirty = true; uiDirty = true;
        }
    }
}

// ---------------- Camera e mapa ----------------
static void updateCamera(void) {
    camX = (px >> 8) + PW / 2 - 120;
    camY = (py >> 8) + PH / 2 - 76;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > W * 8 - 240) camX = W * 8 - 240;
    if (camY > H * 8 - 160) camY = H * 8 - 160;
}

static void drawMap(void) {
    int ctx = camX >> 3, cty = camY >> 3;
    if (!mapDirty && ctx == lastCTX && cty == lastCTY) return;
    lastCTX = ctx; lastCTY = cty; mapDirty = false;
    for (int y = cty; y <= cty + 20; y++)
        for (int x = ctx; x <= ctx + 30; x++) {
            int idx = (y & 31) * 32 + (x & 31);
            int b = (y < H && x < W) ? world[y][x] : B_BEDROCK;
            MAP_FG[idx] = (u16)(b | (iconBank(b) << 12));
            int wall = 0;
            if (x < W && y > surface[x] && (b == B_AIR || b == B_LEAVES || b == B_GLASS || b == B_SAPLING))
                wall = y > surface[x] + 4 ? T_WALL2 : T_WALL;
            MAP_BG[idx] = (u16)wall;
        }
}

// ---------------- Interface ----------------
static void drawSlot(int col, int row, const Slot *s, bool panel) {
    if (!s->id) return;
    if (isTool(s->id)) txtNum(col + 1, row, (toolMaxDur(s->id) - s->dmg) * 99 / toolMaxDur(s->id), 2, panel);
    else if (s->count > 1) txtNum(col + 1, row, s->count, 2, panel);
}

static void itemLine(int row, const Slot *s, bool panel) {
    char name[40];
    if (!s->id) { txt(1, row, "(VAZIO)", panel); return; }
    itemName(s->id, name);
    txt(1, row, name, panel);
    int c = 2 + (int)strlen(name);
    if (isTool(s->id)) {
        txtNum(c, row, toolMaxDur(s->id) - s->dmg, 1, panel);
        int n = toolMaxDur(s->id) - s->dmg;
        int w = n >= 1000 ? 4 : n >= 100 ? 3 : n >= 10 ? 2 : 1;
        txt(c + w, row, "/", panel);
        txtNum(c + w + 1, row, toolMaxDur(s->id), 1, panel);
    } else {
        txt(c, row, "X", panel);
        txtNum(c + 1, row, s->count, 1, panel);
    }
}

static void drawHUD(void) {
    if (uiDirty) {
        txtFill(0, 19, false);
        txtFill(18, 18, true);
        itemLine(0, &inv[selSlot], false);
        if (mineX >= 0 && mineNeed > 0) {
            char bar[19];
            int n = mineProg * 16 / mineNeed;
            bar[0] = '[';
            for (int i = 0; i < 16; i++) bar[1 + i] = i < n ? '#' : '.';
            bar[17] = ']'; bar[18] = 0;
            txt(1, 1, bar, false);
        } else {
            int tx, ty;
            aimTile(&tx, &ty);
            int b = getBlock(tx, ty);
            if (b != B_AIR && blockInfo[b].hard10 != 0xFFFF && !canHarvest(b, inv[selSlot].id))
                txt(1, 1, "PRECISA PICARETA MELHOR", false);
        }
        if (toast && toastTime) txt(1, 2, toast, false);
        for (int i = 0; i < 9; i++) drawSlot(1 + i * 3, 18, &inv[i], true);
        uiDirty = false;
    }
    for (int i = 0; i < 9; i++) sprItem((1 + i * 3) * 8, 18 * 8, inv[i].id);
    spr((1 + selSlot * 3) * 8, 18 * 8, false, OBJT_FRAME, 0, false);
}

static void drawMenu(void) {
    if (uiDirty) {
        txtFill(0, 19, true);
        txt(0, 0, tab == 0 ? "[INVENTARIO]  CRIAR    START:X" : " INVENTARIO  [CRIAR]   START:X", true);
        if (tab == 0) {
            for (int r = 0; r < 4; r++)
                for (int i = 0; i < 9; i++) {
                    int s = r * 9 + i;
                    drawSlot(1 + i * 3, 2 + r * 2, &inv[s], true);
                    if (s == invPick) txt(i * 3, 2 + r * 2, "*", true);
                }
            itemLine(10, &inv[invCur], true);
            txt(1, 12, "A: PEGAR/TROCAR  L/R: ABAS", true);
            txt(1, 13, "B: VOLTAR  SELECT: SALVAR", true);
            txt(1, 15, "COMBUSTIVEL:", true);
            txtNum(14, 15, fuelHalf / 2, 1, true);
            txt(1, 16, "PERTO:", true);
            txt(8, 16, hasTable ? "BANCADA" : "-", true);
            if (hasFurnace) txt(16, 16, "FORNALHA", true);
            if (toast && toastTime) txt(1, 17, toast, true);
            txt(1, 19, "L+R+SELECT: NOVO MUNDO", true);
        } else {
            char name[40];
            for (int i = 0; i < 12 && craftScroll + i < numRecipes; i++) {
                int ri = craftScroll + i;
                const Recipe *r = &recipes[ri];
                itemName(r->out, name);
                name[20] = 0;
                if (ri == craftCur) txt(0, 2 + i, ">", true);
                txt(3, 2 + i, name, true);
                txt(24, 2 + i, "X", true);
                txtNum(25, 2 + i, r->outN, 1, true);
                if (canCraft(ri, hasTable, hasFurnace)) txt(27, 2 + i, "+", true);
                if (r->station) txt(29, 2 + i, r->station == ST_TABLE ? "B" : "F", true);
            }
            const Recipe *r = &recipes[craftCur];
            txt(0, 15, "PRECISA:", true);
            for (int i = 0; i < r->n; i++) {
                itemName(r->inId[i], name);
                name[18] = 0;
                txtNum(1, 16 + i, r->inN[i], 1, true);
                txt(4, 16 + i, name, true);
                txt(23, 16 + i, "TEM", true);
                txtNum(27, 16 + i, countItem(r->inId[i]), 1, true);
            }
            if (r->station == ST_FURNACE) txt(1, 16 + r->n, "+ COMBUSTIVEL", true);
            if (toast && toastTime) txt(16, 15, toast, true);
            txt(0, 19, "A:CRIAR B:VOLTAR +:PODE B/F", true);
        }
        uiDirty = false;
    }
    if (tab == 0) {
        for (int s = 0; s < INV_SIZE; s++) sprItem((1 + (s % 9) * 3) * 8, (2 + (s / 9) * 2) * 8, inv[s].id);
        spr((1 + (invCur % 9) * 3) * 8, (2 + (invCur / 9) * 2) * 8, false, OBJT_FRAME, 0, false);
    } else {
        for (int i = 0; i < 12 && craftScroll + i < numRecipes; i++)
            sprItem(8, (2 + i) * 8, recipes[craftScroll + i].out);
    }
}

static void handleMenu(u16 keys, u16 down) {
    if ((keys & K_L) && (keys & K_R) && (down & K_SELECT)) {
        seed = seed * 1103515245u + 12345u + frameCount;
        invClear(); generateWorld(); spawnPlayer();
        mapDirty = true; menuOpen = false; selSlot = 0;
        setToast("NOVO MUNDO!");
        return;
    }
    if (down & (K_B | K_START)) { menuOpen = false; invPick = -1; uiDirty = true; return; }
    if (down & K_SELECT) { saveGame(); setToast("JOGO SALVO!"); return; }
    if ((down & (K_L | K_R)) && !((keys & K_L) && (keys & K_R))) { tab ^= 1; uiDirty = true; }
    if (tab == 0) {
        int cur = invCur;
        if (down & K_LEFT)  cur = cur % 9 ? cur - 1 : cur + 8;
        if (down & K_RIGHT) cur = cur % 9 != 8 ? cur + 1 : cur - 8;
        if (down & K_UP)    cur = (cur + 27) % 36;
        if (down & K_DOWN)  cur = (cur + 9) % 36;
        if (cur != invCur) { invCur = cur; uiDirty = true; }
        if (down & K_A) {
            if (invPick < 0) invPick = invCur;
            else {
                Slot a = inv[invPick], *b = &inv[invCur];
                if (a.id == b->id && !isTool(a.id) && invPick != invCur) {
                    int k = 64 - b->count; if (k > a.count) k = a.count;
                    b->count += k; inv[invPick].count -= k;
                    if (!inv[invPick].count) inv[invPick].id = 0;
                } else { inv[invPick] = *b; *b = a; }
                invPick = -1;
            }
            uiDirty = true;
        }
    } else {
        int cur = craftCur;
        if (down & K_UP)    cur = cur > 0 ? cur - 1 : numRecipes - 1;
        if (down & K_DOWN)  cur = cur < numRecipes - 1 ? cur + 1 : 0;
        if (down & K_LEFT)  cur = cur >= 12 ? cur - 12 : 0;
        if (down & K_RIGHT) cur = cur + 12 < numRecipes ? cur + 12 : numRecipes - 1;
        if (cur != craftCur) {
            craftCur = cur;
            if (craftCur < craftScroll) craftScroll = craftCur;
            if (craftCur >= craftScroll + 12) craftScroll = craftCur - 11;
            uiDirty = true;
        }
        if (down & K_A) {
            if (craft(craftCur, hasTable, hasFurnace)) setToast("CRIADO!");
            uiDirty = true;
        }
    }
}

// ---------------- Loop ----------------
void gameInit(void) {
    setupGraphics();
    initRecipes();
    saplingTicks = saplingRandomTicks(W * H);
    invClear();
    if (!loadGame()) generateWorld();
    spawnPlayer();
    updateCamera();
}

void gameFrame(u16 keys) {
    u16 down = keys & ~prevKeys;
    prevKeys = keys;
    frameCount++;

    if (menuOpen) {
        handleMenu(keys, down);
        keys = 0; down = 0;     // jogador fica parado com o menu aberto
    } else {
        if (down & K_START) { menuOpen = true; tab = 0; invCur = selSlot; invPick = -1; uiDirty = true; stopMining(); }
        else if (down & K_SELECT) { selSlot = (selSlot + 1) % 9; uiDirty = true; stopMining(); }
    }

    updatePlayer(keys, down);
    if (!menuOpen) editBlocks(keys, down);
    randomTicks();
    updateCamera();

    if ((frameCount & 15) == 0) {
        bool t = nearBlock(B_CTABLE), f = nearBlock(B_FURNACE);
        if (t != hasTable || f != hasFurnace) { hasTable = t; hasFurnace = f; uiDirty = true; }
    }
    if (toastTime && !--toastTime) uiDirty = true;
}

void gameDraw(void) {   // chamado no VBlank
    drawMap();
    REG_BG0HOFS = (u16)camX; REG_BG0VOFS = (u16)camY;
    REG_BG1HOFS = (u16)camX; REG_BG1VOFS = (u16)camY;
    sprReset();
    if (menuOpen) {
        drawMenu();
    } else {
        drawHUD();
        int sx = (px >> 8) - camX - 1, sy = (py >> 8) - camY - 2;
        spr(sx, sy, true, OBJT_PLAYER, 0, facingLeft);
        int tx, ty;
        aimTile(&tx, &ty);
        bool blink = mineX >= 0 && (mineProg & 8);
        if (!blink) spr(tx * 8 - camX, ty * 8 - camY, false, OBJT_FRAME, 0, false);
    }
    for (int i = 0; i < 128 * 4; i++) OAM[i] = oamShadow[i];
}

#ifndef HOST_PREVIEW
int main(void) {
    gameInit();
    while (1) {
        while (REG_VCOUNT >= 160);
        while (REG_VCOUNT < 160);
        gameDraw();
        gameFrame((u16)(~REG_KEYINPUT & 0x03FF));
    }
}
#endif
