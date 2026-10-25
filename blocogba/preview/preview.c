// Gera prints do BlocoGBA rodando o codigo real do jogo no PC
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
uint16_t h_regs[0x200], h_pal[512], h_vram[0x18000 / 2], h_oam[512];
uint8_t h_sram[0x10000];
#include "../source/main.c"

static int pix4(uint32_t *tiles, int tile, int x, int y) { return (tiles[tile * 8 + y] >> (x * 4)) & 15; }

// retorna indice de paleta (0 = transparente)
static int bgPixel(int n, int sx, int sy) {
    uint16_t cnt = h_regs[4 + n];
    int sbb = (cnt >> 8) & 31, cbb = (cnt >> 2) & 3;
    int x = (sx + h_regs[8 + n * 2]) & 255, y = (sy + h_regs[9 + n * 2]) & 255;
    uint16_t e = h_vram[sbb * 0x400 + (y >> 3) * 32 + (x >> 3)];
    uint32_t *tiles = (uint32_t *)((uint8_t *)h_vram + cbb * 0x4000);
    int tx = x & 7, ty = y & 7;
    if (e & 0x400) tx = 7 - tx;
    if (e & 0x800) ty = 7 - ty;
    int p = pix4(tiles, e & 1023, tx, ty);
    return p ? (e >> 12) * 16 + p : 0;
}

static void shot(const char *name) {
    static uint8_t img[160][240][3];
    uint32_t *ot = (uint32_t *)((uint8_t *)h_vram + 0x10000);
    for (int y = 0; y < 160; y++)
        for (int x = 0; x < 240; x++) {
            uint16_t c = h_pal[0];
            int v;
            if ((v = bgPixel(1, x, y))) c = h_pal[v];
            if ((v = bgPixel(0, x, y))) c = h_pal[v];
            if ((v = bgPixel(2, x, y))) c = h_pal[v];
            for (int i = 127; i >= 0; i--) {
                uint16_t a0 = h_oam[i * 4], a1 = h_oam[i * 4 + 1], a2 = h_oam[i * 4 + 2];
                if (a0 & 0x200) continue;
                int sy = a0 & 255, sx = a1 & 511, shape = a0 >> 14;
                if (sy >= 160) sy -= 256;
                if (sx >= 240) sx -= 512;
                int hgt = shape == 2 ? 16 : 8;
                int lx = x - sx, ly = y - sy;
                if (lx < 0 || ly < 0 || lx >= 8 || ly >= hgt) continue;
                if (a1 & 0x1000) lx = 7 - lx;
                int p = pix4(ot, (a2 & 1023) + (ly >> 3), lx, ly & 7);
                if (p) c = h_pal[256 + (a2 >> 12) * 16 + p];
            }
            img[y][x][0] = (c & 31) * 255 / 31; img[y][x][1] = ((c >> 5) & 31) * 255 / 31; img[y][x][2] = ((c >> 10) & 31) * 255 / 31;
        }
    char fn[64];
    snprintf(fn, sizeof fn, "%s.ppm", name);
    FILE *f = fopen(fn, "wb"); fprintf(f, "P6 240 160 255\n"); fwrite(img, 1, sizeof img, f); fclose(f);
}

static void run(uint16_t keys, int frames) { while (frames-- > 0) { gameDraw(); gameFrame(keys); } gameDraw(); }
static void tap(uint16_t keys) { run(keys, 1); run(0, 4); }

static void goTo(int tx, int ty) {   // pes do jogador no bloco (tx, ty)
    px = (tx * 8 + 1) << 8; py = (ty * 8 + 8 - PH) << 8; vy = 0;
    mapDirty = true; uiDirty = true;
}

int main(void) {
    gameInit();
    run(0, 20);

    // 1) socando uma arvore
    int tree = W / 2;
    while (tree < W - 4 && world[surface[tree] - 1][tree] != B_LOG) tree++;
    int sx = tree - 2;
    goTo(sx, surface[sx] - 1);
    run(0, 10);
    facingLeft = false; curDX = 2; curDY = -1;
    addItem(B_LOG, 2);
    run(K_R, 100);
    shot("gba1_coletando");
    run(K_R, 100);
    run(0, 10);

    // 2) bancada e crafting
    addItem(B_COBBLE, 10);
    int bx = sx - 2;
    world[surface[bx] - 1][bx] = B_CTABLE; mapDirty = true;
    run(0, 20);
    tap(K_START);
    tap(K_R);                       // aba criar
    tap(K_A); tap(K_A);             // tabuas
    tap(K_DOWN); tap(K_A);          // gravetos
    for (int i = 0; i < 7; i++) tap(K_DOWN);
    tap(K_A);                       // picareta de pedra
    run(0, 5);
    shot("gba2_criando");
    tap(K_B);

    // 3) caverna funda com minerios
    int best = -1, cx = 0, cy = 0;
    for (int x = 10; x < W - 10; x++)
        for (int y = 40; y < H - 4; y++) {
            if (world[y][x] || world[y - 1][x] || !solid(x, y + 1)) continue;
            int sc = 0;
            for (int dx = -14; dx <= 14; dx++)
                for (int dy = -9; dy <= 9; dy++) {
                    int b = getBlock(x + dx, y + dy);
                    if (b == B_AIR) sc++;
                    if (b >= B_COAL_ORE && b <= B_EMERALD_ORE) sc += b == B_DIAMOND_ORE ? 30 : 6;
                }
            if (sc > best) { best = sc; cx = x; cy = y; }
        }
    goTo(cx, cy);
    inv[1].id = TOOL(M_IRON, T_PICK); inv[1].count = 1; inv[1].dmg = 40;
    selSlot = 1; uiDirty = true;
    run(0, 10);
    // mira num minerio ao alcance
    int ox = 0, oy = 0, ov = -1, ptx, pty;
    playerCenterTile(&ptx, &pty);
    for (int dx = -4; dx <= 4; dx++)
        for (int dy = -4; dy <= 4; dy++) {
            int b = getBlock(ptx + dx, pty + dy);
            if (b < B_COAL_ORE || b > B_EMERALD_ORE) continue;
            int v = (b == B_DIAMOND_ORE ? 100 : (b - B_COAL_ORE) * 10) - abs(dx) - abs(dy);
            if (v > ov) { ov = v; ox = dx; oy = dy; }
        }
    if (ov >= 0) { curDX = ox; curDY = oy; facingLeft = ox < 0; }
    run(K_R, 20);
    shot("gba3_caverna");
    run(K_R, 60);
    run(0, 10);

    // 4) inventario
    addItem(I_DIAMOND, 3); addItem(I_RAW_IRON, 7); addItem(I_COAL, 21); addItem(I_REDSTONE, 9);
    addItem(I_LAPIS, 6); addItem(B_GLASS, 8); addItem(TOOL(M_DIAMOND, T_SWORD), 1);
    addItem(I_GOLD, 4); addItem(I_EMERALD, 2); addItem(I_APPLE, 1); addItem(I_FLINT, 2);
    addItem(B_IRON_BLOCK, 1); addItem(TOOL(M_GOLD, T_AXE), 1);
    tap(K_START);
    tap(K_DOWN); tap(K_RIGHT); tap(K_RIGHT);
    shot("gba4_inventario");
    return 0;
}
