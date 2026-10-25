// BlocoDS - clone de blocos estilo Minecraft para Nintendo DS
// C + libnds (BlocksDS ou devkitPro). Regras do jogo em blococore (core.c),
// graficos 2D compartilhados com o GBA em blococore (art.c).
//
// Tela de cima: mundo 3D.
// Tela de baixo: interface grafica por toque com 4 abas
//   MUNDO   - HUD, minimapa (com modo caverna), mira, barra rapida
//   MOCHILA - inventario de 36 slots com detalhes do item
//   CRIAR   - receitas com icones, filtro "possiveis", bancada e fornalha
//   MENU    - salvar, carregar, novo mundo e opcoes
//
// Otimizacoes: ponto fixo, mascara de faces expostas, culling por raio,
// direcao e face, limite de quads, UI redesenhada so quando muda.

#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "core.h"
#include "art.h"

// ---------------- Mundo ----------------
#define WX 48
#define WY 32
#define WZ 48
#define MAX_QUADS 1450

typedef int32_t fx;   // ponto fixo 20.12
#define FX(n)   ((fx)((n) * 4096))
#define FXF(f)  ((fx)((f) * 4096))
static inline fx fmul(fx a, fx b) { return (fx)(((int64_t)a * b) >> 12); }

#define SKY_R 16
#define SKY_G 24
#define SKY_B 31
static int fogR = SKY_R, fogG = SKY_G, fogB = SKY_B;

// opcoes
static int optSens = 2;        // 1..3
static bool optInvertY = false;
static int renderR = 8;        // 6..8

static u8 world[WX][WY][WZ];
static u8 faces[WX][WY][WZ];   // bits: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
static u8 heightMap[WX][WZ];
static bool mapDirty = true;   // minimapa precisa redesenhar

static inline bool inBounds(int x, int y, int z) {
    return x >= 0 && x < WX && y >= 0 && y < WY && z >= 0 && z < WZ;
}
static inline int getBlock(int x, int y, int z) {
    if (y >= WY) return B_AIR;
    if (!inBounds(x, y, z)) return B_BEDROCK;
    return world[x][y][z];
}
static inline bool solid(int x, int y, int z) { return getBlock(x, y, z) != B_AIR; }

static void computeFaces(int x, int y, int z) {
    if (!inBounds(x, y, z)) return;
    u8 m = 0;
    if (world[x][y][z] != B_AIR) {
        if (!solid(x + 1, y, z)) m |= 1;
        if (!solid(x - 1, y, z)) m |= 2;
        if (!solid(x, y + 1, z)) m |= 4;
        if (!solid(x, y - 1, z)) m |= 8;
        if (!solid(x, y, z + 1)) m |= 16;
        if (!solid(x, y, z - 1)) m |= 32;
    }
    faces[x][y][z] = m;
}
static void computeAllFaces(void) {
    for (int x = 0; x < WX; x++)
        for (int y = 0; y < WY; y++)
            for (int z = 0; z < WZ; z++) computeFaces(x, y, z);
}

static void setBlock(int x, int y, int z, int t) {
    if (!inBounds(x, y, z)) return;
    world[x][y][z] = (u8)t;
    computeFaces(x, y, z);
    computeFaces(x + 1, y, z); computeFaces(x - 1, y, z);
    computeFaces(x, y + 1, z); computeFaces(x, y - 1, z);
    computeFaces(x, y, z + 1); computeFaces(x, y, z - 1);
    mapDirty = true;
}

// ---------------- Geracao ----------------
static u32 seed = 1;

static int hash3(int x, int y, int z) {
    u32 h = (u32)x * 374761393u + (u32)y * 3266489917u + (u32)z * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (int)((h ^ (h >> 16)) & 255);
}
static int noise2(int x, int z, int cell) {
    int gx = x / cell, gz = z / cell;
    int tx = (x % cell) * 256 / cell, tz = (z % cell) * 256 / cell;
    int a = hash3(gx, 0, gz), b = hash3(gx + 1, 0, gz), c = hash3(gx, 0, gz + 1), d = hash3(gx + 1, 0, gz + 1);
    int ab = a + ((b - a) * tx >> 8), cd = c + ((d - c) * tx >> 8);
    return ab + ((cd - ab) * tz >> 8);
}
static int noise3(int x, int y, int z, int cell) {
    int gx = x / cell, gy = y / cell, gz = z / cell;
    int tx = (x % cell) * 256 / cell, ty = (y % cell) * 256 / cell, tz = (z % cell) * 256 / cell;
    int v[2];
    for (int k = 0; k < 2; k++) {
        int a = hash3(gx, gy + k, gz), b = hash3(gx + 1, gy + k, gz);
        int c = hash3(gx, gy + k, gz + 1), d = hash3(gx + 1, gy + k, gz + 1);
        int ab = a + ((b - a) * tx >> 8), cd = c + ((d - c) * tx >> 8);
        v[k] = ab + ((cd - ab) * tz >> 8);
    }
    return v[0] + ((v[1] - v[0]) * ty >> 8);
}

static void vein(int ore, int count, int ymin, int ymax, int size, int replace) {
    for (int i = 0; i < count; i++) {
        int x = (int)(rnd() % WX), z = (int)(rnd() % WZ);
        int y = ymin + (int)(rnd() % (u32)(ymax - ymin + 1));
        for (int s = 0; s < size; s++) {
            if (inBounds(x, y, z) && (world[x][y][z] == B_STONE || (replace && world[x][y][z] == replace)))
                world[x][y][z] = (u8)ore;
            switch (rnd() % 6) {
            case 0: x++; break; case 1: x--; break; case 2: y++; break;
            case 3: y--; break; case 4: z++; break; default: z--; break;
            }
        }
    }
}

static void placeTree(int x, int y, int z) {   // carvalho: tronco 4-6, copa em camadas
    int h = 4 + (int)(rnd() % 3);
    for (int i = 1; i <= h; i++) world[x][y + i][z] = B_LOG;
    int top = y + h;
    for (int ly = top - 2; ly <= top + 1; ly++) {
        int r = ly >= top ? 1 : 2;
        for (int lx = x - r; lx <= x + r; lx++)
            for (int lz = z - r; lz <= z + r; lz++) {
                if (!inBounds(lx, ly, lz) || world[lx][ly][lz] != B_AIR) continue;
                bool corner = (lx - x == r || x - lx == r) && (lz - z == r || z - lz == r);
                if (corner && (ly == top + 1 || (rnd() & 1))) continue;
                world[lx][ly][lz] = B_LEAVES;
            }
    }
}

static void generateWorld(void) {
    rndSeed(seed * 2654435761u + 1);
    for (int x = 0; x < WX; x++)
        for (int z = 0; z < WZ; z++) {
            int h = 13 + (noise2(x, z, 12) * 5 + noise2(x, z, 6) * 2 + noise2(x + 64, z + 64, 24) * 4) / 255;
            heightMap[x][z] = (u8)h;
            for (int y = 0; y < WY; y++) {
                int t = B_AIR;
                if (y == 0 || (y <= 2 && hash3(x, y, z) < 110)) t = B_BEDROCK;
                else if (y < h - 3) t = B_STONE;
                else if (y < h) t = B_DIRT;
                else if (y == h) t = h <= 14 ? B_SAND : B_GRASS;
                if (t != B_BEDROCK && t != B_AIR && y >= 3 && y < h - 2) {
                    int n = noise3(x, y * 2, z, 8) + noise3(x + 100, y, z + 100, 4) / 3;
                    if (n > 215) t = B_AIR;
                }
                world[x][y][z] = (u8)t;
            }
        }
    // distribuicao inspirada na 1.18+ (comprimida para 32 de altura)
    vein(B_GRAVEL,      14, 4, 18, 14, B_DIRT);
    vein(B_COAL_ORE,    60, 5, 20, 10, 0);
    vein(B_IRON_ORE,    50, 2, 18, 8, 0);
    vein(B_LAPIS_ORE,   12, 2, 12, 6, 0);
    vein(B_GOLD_ORE,    16, 2, 10, 7, 0);
    vein(B_REDSTONE_ORE,20, 1,  7, 7, 0);
    vein(B_DIAMOND_ORE, 12, 1,  6, 5, 0);
    vein(B_EMERALD_ORE, 24, 8, 18, 1, 0);
    for (int x = 3; x < WX - 3; x++)
        for (int z = 3; z < WZ - 3; z++) {
            int h = heightMap[x][z];
            if (world[x][h][z] == B_GRASS && h + 9 < WY && hash3(x * 7, 5, z * 13) < 7)
                placeTree(x, h, z);
        }
    computeAllFaces();
    mapDirty = true;
}

// ---------------- Jogador ----------------
#define P_HW   FXF(0.3)
#define P_H    FXF(1.75)
#define P_EYE  FXF(1.6)
#define GRAV   FXF(0.012)
#define JUMP_V FXF(0.17)
#define SPEED  FXF(0.075)

static fx px, py, pz, vy;
static int yaw = 0, pitch = 0;
static bool onGround = false;
static int selSlot = 0;

static bool collides(fx x, fx y, fx z) {
    int x0 = (x - P_HW) >> 12, x1 = (x + P_HW - 1) >> 12;
    int y0 = y >> 12,          y1 = (y + P_H - 1) >> 12;
    int z0 = (z - P_HW) >> 12, z1 = (z + P_HW - 1) >> 12;
    for (int ix = x0; ix <= x1; ix++)
        for (int iy = y0; iy <= y1; iy++)
            for (int iz = z0; iz <= z1; iz++)
                if (solid(ix, iy, iz)) return true;
    return false;
}

static void spawnPlayer(void) {
    int cx = WX / 2, cz = WZ / 2, top = heightMap[cx][cz];
    for (int r = 0; r < WX / 2; r++)
        for (int dx = -r; dx <= r; dx++)
            for (int dz = -r; dz <= r; dz++) {
                int x = WX / 2 + dx, z = WZ / 2 + dz;
                if (x < 1 || z < 1 || x >= WX - 1 || z >= WZ - 1) continue;
                int t = heightMap[x][z];
                if (world[x][t][z] == B_GRASS && world[x][t + 1][z] == B_AIR && world[x][t + 2][z] == B_AIR) {
                    cx = x; cz = z; top = t;
                    goto found;
                }
            }
found:
    px = FX(cx) + FXF(0.5);
    pz = FX(cz) + FXF(0.5);
    py = FX(top + 1);
    vy = 0;
}

static void movePlayer(fx dx, fx dz) {
    if (dx && !collides(px + dx, py, pz)) px += dx;
    if (dz && !collides(px, py, pz + dz)) pz += dz;
    vy -= GRAV;
    if (vy < -FXF(0.5)) vy = -FXF(0.5);
    onGround = false;
    if (!collides(px, py + vy, pz)) {
        py += vy;
    } else {
        if (vy < 0) {
            onGround = true;
            py = ((py + vy) >> 12 << 12) + FX(1);
            if (collides(px, py, pz)) py -= FX(1);
        }
        vy = 0;
    }
    if (py < FX(-10)) spawnPlayer();
}

// ---------------- Camera e mira ----------------
static fx fwdX, fwdY, fwdZ;

static void updateCamera(void) {
    fx sy = sinLerp((s16)yaw), cy = cosLerp((s16)yaw);
    fx sp = sinLerp((s16)pitch), cp = cosLerp((s16)pitch);
    fwdX = fmul(cp, sy);
    fwdY = -sp;
    fwdZ = -fmul(cp, cy);
}

static int hitX, hitY, hitZ, prevX, prevY, prevZ;
static bool hasHit = false;

static void raycast(void) {
    fx x = px, y = py + P_EYE, z = pz;
    int lx = x >> 12, ly = y >> 12, lz = z >> 12;
    hasHit = false;
    for (int i = 0; i < 80; i++) {          // alcance de 5 blocos
        x += fwdX >> 4; y += fwdY >> 4; z += fwdZ >> 4;
        int cx = x >> 12, cy = y >> 12, cz = z >> 12;
        if (cx == lx && cy == ly && cz == lz) continue;
        if (inBounds(cx, cy, cz) && world[cx][cy][cz] != B_AIR) {
            hitX = cx; hitY = cy; hitZ = cz;
            prevX = lx; prevY = ly; prevZ = lz;
            hasHit = true;
            return;
        }
        lx = cx; ly = cy; lz = cz;
    }
}

static bool isUnderground(void) {
    int cx = px >> 12, cy = (py + P_EYE) >> 12, cz = pz >> 12;
    return cx >= 0 && cx < WX && cz >= 0 && cz < WZ && cy < heightMap[cx][cz] - 1;
}

// ---------------- Mineracao ----------------
static int mineX = -1, mineY, mineZ, mineProg, mineNeed, mineCooldown;
static void stopMining(void) { mineX = -1; mineProg = 0; }

// ---------------- Renderizacao 3D ----------------
#define S 512   // 1 bloco = 0.125 em v16; a matriz escala x8 (mundo ate 63 blocos)

static int quadCount;

static u16 shadeColor(const u8 *c, int face, int x, int y, int z, int dist2, int bright, int dark) {
    static const int shade[6] = {26, 26, 32, 16, 22, 22};
    int s = shade[face];
    if ((x ^ y ^ z) & 1) s = s * 29 / 32;
    s = s * (32 - dark) / 32;
    int r = (c[0] * s >> 5) + bright, g = (c[1] * s >> 5) + bright, b = (c[2] * s >> 5) + bright;
    const int R2 = renderR * renderR;
    int f = (dist2 - R2 / 2) * 32 / (R2 - R2 / 2);
    if (f > 0) {
        if (f > 32) f = 32;
        r += (fogR - r) * f >> 5; g += (fogG - g) * f >> 5; b += (fogB - b) * f >> 5;
    }
    if (r > 31) r = 31;
    if (g > 31) g = 31;
    if (b > 31) b = 31;
    return RGB15(r, g, b);
}

static void drawRect(int f, int x, int y, int z, int a0, int a1, int b0, int b1, int off, u16 col) {
    int X = x * S, Y = y * S, Z = z * S;
    glColor(col);
    switch (f) {
    case 0: case 1: {
        v16 p = (v16)(f == 0 ? X + S + off : X - off);
        glVertex3v16(p, (v16)(Y + b0), (v16)(Z + a0)); glVertex3v16(p, (v16)(Y + b1), (v16)(Z + a0));
        glVertex3v16(p, (v16)(Y + b1), (v16)(Z + a1)); glVertex3v16(p, (v16)(Y + b0), (v16)(Z + a1));
        break; }
    case 2: case 3: {
        v16 p = (v16)(f == 2 ? Y + S + off : Y - off);
        glVertex3v16((v16)(X + a0), p, (v16)(Z + b0)); glVertex3v16((v16)(X + a0), p, (v16)(Z + b1));
        glVertex3v16((v16)(X + a1), p, (v16)(Z + b1)); glVertex3v16((v16)(X + a1), p, (v16)(Z + b0));
        break; }
    default: {
        v16 p = (v16)(f == 4 ? Z + S + off : Z - off);
        glVertex3v16((v16)(X + a0), (v16)(Y + b0), p); glVertex3v16((v16)(X + a0), (v16)(Y + b1), p);
        glVertex3v16((v16)(X + a1), (v16)(Y + b1), p); glVertex3v16((v16)(X + a1), (v16)(Y + b0), p);
        break; }
    }
    quadCount++;
}

static void drawBlockFace(int t, int f, int x, int y, int z, int d2, bool hl, int dark) {
    const BlockInfo *bi = &blockInfo[t];
    int bright = hl ? 5 : 0;
    drawRect(f, x, y, z, 0, S, 0, S, 0, shadeColor(f == 2 ? bi->top : bi->side, f, x, y, z, d2, bright, dark));
    if (d2 > 30) return;   // detalhes so de perto
    if (bi->detKind == DET_SPOT && !(t == B_CTABLE && f != 2) && !(t == B_FURNACE && (f == 2 || f == 3))) {
        drawRect(f, x, y, z, S / 4, 3 * S / 4, S / 4, 3 * S / 4, 8, shadeColor(bi->det, f, x, y, z, d2, bright, dark));
    } else if (bi->detKind == DET_BAND && f != 2 && f != 3) {
        drawRect(f, x, y, z, 0, S, 3 * S / 4, S, 8, shadeColor(bi->det, f, x, y, z, d2, bright, dark));
    } else if (t == B_LOG && (f == 2 || f == 3)) {
        drawRect(f, x, y, z, S / 4, 3 * S / 4, S / 4, 3 * S / 4, 8, shadeColor(bi->det, f, x, y, z, d2, bright, dark));
    }
}

static void renderWorld(void) {
    fx ex = px, ey = py + P_EYE, ez = pz;
    int cx = ex >> 12, cy = ey >> 12, cz = ez >> 12;

    bool under = isUnderground();
    int tr = under ? 2 : SKY_R, tg = under ? 2 : SKY_G, tb = under ? 4 : SKY_B;
    fogR += (tr > fogR) - (tr < fogR);
    fogG += (tg > fogG) - (tg < fogG);
    fogB += (tb > fogB) - (tb < fogB);
    glClearColor((u8)fogR, (u8)fogG, (u8)fogB, 31);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAtf32(ex, ey, ez, ex + fwdX, ey + fwdY, ez + fwdZ, 0, FX(1), 0);
    glScalef32(FX(8), FX(8), FX(8));
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBegin(GL_QUADS);
    quadCount = 0;

    int R = renderR;
    int xa = cx - R, xb = cx + R, ya = cy - R, yb = cy + R, za = cz - R, zb = cz + R;
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (za < 0) za = 0;
    if (xb >= WX) xb = WX - 1;
    if (yb >= WY) yb = WY - 1;
    if (zb >= WZ) zb = WZ - 1;
    const int R2 = R * R;
    int mineDark = mineX >= 0 && mineNeed > 0 ? mineProg * 20 / mineNeed : 0;

    for (int x = xa; x <= xb; x++)
        for (int z = za; z <= zb; z++) {
            int dx = x - cx, dz = z - cz, dxz = dx * dx + dz * dz;
            if (dxz > R2) continue;
            fx bx = FX(x) + FXF(0.5) - ex, bz = FX(z) + FXF(0.5) - ez;
            if (fmul(bx, fwdX) + fmul(bz, fwdZ) < -FXF(1.5)) continue;
            for (int y = ya; y <= yb; y++) {
                u8 m = faces[x][y][z];
                if (!m) continue;
                int dy = y - cy, d2 = dxz + dy * dy;
                if (d2 > R2) continue;
                if (ex <= FX(x + 1)) m &= ~1;
                if (ex >= FX(x))     m &= ~2;
                if (ey <= FX(y + 1)) m &= ~4;
                if (ey >= FX(y))     m &= ~8;
                if (ez <= FX(z + 1)) m &= ~16;
                if (ez >= FX(z))     m &= ~32;
                if (!m) continue;
                int t = world[x][y][z];
                bool hl = hasHit && x == hitX && y == hitY && z == hitZ;
                int dark = (x == mineX && y == mineY && z == mineZ) ? mineDark : 0;
                for (int f = 0; f < 6; f++) {
                    if (!(m & (1 << f))) continue;
                    drawBlockFace(t, f, x, y, z, d2, hl, dark);
                    if (quadCount >= MAX_QUADS) goto done;
                }
            }
        }
done:
    glEnd();
}

static void renderCrosshair(void) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
    glBegin(GL_QUADS);
    glColor(RGB15(31, 31, 31));
    const v16 zz = -floattov16(0.15f), w = 22, h = 4;
    glVertex3v16(-w, -h, zz); glVertex3v16(w, -h, zz); glVertex3v16(w, h, zz); glVertex3v16(-w, h, zz);
    glVertex3v16(-h, -w, zz); glVertex3v16(h, -w, zz); glVertex3v16(h, w, zz); glVertex3v16(-h, w, zz);
    glEnd();
}

static bool nearBlock(int type) {
    int cx = px >> 12, cy = (py + P_EYE) >> 12, cz = pz >> 12;
    for (int x = cx - 4; x <= cx + 4; x++)
        for (int y = cy - 4; y <= cy + 4; y++)
            for (int z = cz - 4; z <= cz + 4; z++)
                if (inBounds(x, y, z) && world[x][y][z] == type) return true;
    return false;
}

// =====================================================================
//  Tela de baixo: motor 2D secundario (BG0 texto, BG1 paineis/minimapa)
// =====================================================================
#define UI_TILES   ((volatile u32 *)BG_GFX_SUB)
#define UI_TXT     ((volatile u16 *)BG_GFX_SUB + 28 * 0x400)
#define UI_PNL     ((volatile u16 *)BG_GFX_SUB + 29 * 0x400)
#define UI_OBJ     ((volatile u32 *)SPRITE_GFX_SUB)

// tiles de BG
enum { T_EMPTY = 0, T_PANEL = 1, T_BUTTON = 10, T_SLOT = 19, T_BAR = 23, T_FONT = 64, T_MAP = 128 };
// bancos de paleta da UI
enum { P_UI = 0, P_GOLD = 1, P_GREY = 2, P_GREEN = 3, P_RED = 4, P_SEL = 5, P_MAP = 6, P_BLUE = 7 };
// tiles de sprite: icone 16x16 = id*4; 8x8 = 512+id
#define OBJ_ICON16(id) ((id) * 4)
#define OBJ_ICON8(id)  (512 + (id))
#define OBJ_FRAME16    480
#define OBJ_DOT        640
#define OBJ_VIEW       641

#define C(r, g, b) ART_RGB(r, g, b)
static const u16 uiPal[16] = {
    C(4,4,7), C(1,1,2), C(7,7,11), C(10,10,15), C(13,13,19), C(20,20,26), C(31,31,31), C(20,20,23),
    C(7,20,7), C(14,28,10), C(31,26,8), C(28,7,7), C(9,15,29), C(3,3,5), C(9,24,6), C(16,24,31),
};
// cores do minimapa (banco 6)
enum { MC_VOID = 1, MC_GRASS, MC_GRASS2, MC_LEAF, MC_SAND, MC_STONE, MC_STONE2, MC_DIRT, MC_WOOD, MC_ORE, MC_METAL, MC_GLASS, MC_DEEP, MC_RED, MC_BLUE };
static const u16 mapPal[16] = {
    C(0,0,0), C(2,2,4), C(9,24,6), C(6,18,4), C(4,14,4), C(28,26,16), C(17,17,18), C(11,11,12),
    C(15,10,5), C(22,16,8), C(31,26,8), C(27,27,28), C(22,28,31), C(5,5,8), C(28,6,6), C(10,16,30),
};

static void uiPalettes(void) {
    for (int b = 0; b < 8; b++)
        for (int i = 0; i < 16; i++) BG_PALETTE_SUB[b * 16 + i] = uiPal[i];
    BG_PALETTE_SUB[P_GOLD * 16 + 6]  = uiPal[10];
    BG_PALETTE_SUB[P_GREY * 16 + 6]  = uiPal[7];
    BG_PALETTE_SUB[P_GREEN * 16 + 6] = uiPal[9];
    BG_PALETTE_SUB[P_RED * 16 + 6]   = uiPal[11];
    BG_PALETTE_SUB[P_BLUE * 16 + 6]  = C(14, 20, 31);
    BG_PALETTE_SUB[P_SEL * 16 + 3]   = uiPal[8];     // botao selecionado: verde
    BG_PALETTE_SUB[P_SEL * 16 + 4]   = uiPal[9];
    BG_PALETTE_SUB[P_SEL * 16 + 5]   = uiPal[10];    // borda dourada
    for (int i = 0; i < 16; i++) BG_PALETTE_SUB[P_MAP * 16 + i] = mapPal[i];
    for (int i = 0; i < 16; i++) { SPRITE_PALETTE_SUB[i] = artPal[0][i]; SPRITE_PALETTE_SUB[16 + i] = artPal[1][i]; }
}

// 9-slice: borda clara (5), sombra interna (13), preenchimento fill; bevel = linha clara no topo
static int sliceColor(int part, int x, int y, int fill, bool bevel) {
    int col = part % 3, row = part / 3;
    bool L = col == 0 && x == 0, R = col == 2 && x == 7, T = row == 0 && y == 0, B = row == 2 && y == 7;
    if (L || R || T || B) {
        bool corner = (col != 1 && row != 1) && ((x == 0 || x == 7) && (y == 0 || y == 7));
        return corner ? 0 : 5;
    }
    bool L2 = col == 0 && x == 1, T2 = row == 0 && y == 1, R2 = col == 2 && x == 6, B2 = row == 2 && y == 6;
    if (bevel && T2) return 4;
    if (R2 || B2) return 13;
    if (L2 || T2) return bevel ? 4 : 13;
    return fill;
}

static void uiTiles(void) {
    for (int i = 0; i < 8; i++) UI_TILES[i] = 0;
    for (int p = 0; p < 9; p++)
        for (int y = 0; y < 8; y++) {
            u32 a = 0, b = 0;
            for (int x = 0; x < 8; x++) {
                a |= (u32)sliceColor(p, x, y, 2, false) << (x * 4);
                b |= (u32)sliceColor(p, x, y, 3, true) << (x * 4);
            }
            UI_TILES[(T_PANEL + p) * 8 + y] = a;
            UI_TILES[(T_BUTTON + p) * 8 + y] = b;
        }
    // slot 16x16: moldura escura com borda
    for (int q = 0; q < 4; q++)
        for (int y = 0; y < 8; y++) {
            u32 row = 0;
            for (int x = 0; x < 8; x++) {
                int gx = (q & 1) * 8 + x, gy = (q >> 1) * 8 + y;
                int c = (gx == 0 || gy == 0 || gx == 15 || gy == 15) ? 5 : ((gx == 1 || gy == 1) ? 13 : 1);
                row |= (u32)c << (x * 4);
            }
            UI_TILES[(T_SLOT + q) * 8 + y] = row;
        }
    // barra de progresso: vazia, cheia
    for (int k = 0; k < 2; k++)
        for (int y = 0; y < 8; y++) {
            u32 row = 0;
            for (int x = 0; x < 8; x++) {
                int c = (y == 1 || y == 6) ? 5 : (y > 1 && y < 6 ? (k ? (y == 2 ? 9 : 8) : 1) : 0);
                row |= (u32)c << (x * 4);
            }
            UI_TILES[(T_BAR + k) * 8 + y] = row;
        }
    for (int g = 0; g < 64; g++) artGlyph(UI_TILES + (T_FONT + g) * 8, 32 + g, 6, 1, 0);

    // sprites: icones 16x16 e 8x8 de todos os blocos e itens
    for (int id = 1; id < NUM_BLOCKS; id++) { artTile16(UI_OBJ + OBJ_ICON16(id) * 8, id); artTile(UI_OBJ + OBJ_ICON8(id) * 8, id); }
    for (int id = I_STICK; id <= I_LAST_SIMPLE; id++) { artTile16(UI_OBJ + OBJ_ICON16(id) * 8, id); artTile(UI_OBJ + OBJ_ICON8(id) * 8, id); }
    for (int id = I_TOOLS; id < I_TOOLS_END; id++) { artTile16(UI_OBJ + OBJ_ICON16(id) * 8, id); artTile(UI_OBJ + OBJ_ICON8(id) * 8, id); }
    for (int t = 0; t < 4; t++)
        for (int y = 0; y < 8; y++) {
            u32 row = 0;
            for (int x = 0; x < 8; x++) {
                int gx = (t & 1) * 8 + x, gy = (t >> 1) * 8 + y;
                int c = (gx < 5 || gx > 10) && (gy < 5 || gy > 10) && (gx < 2 || gx > 13 || gy < 2 || gy > 13) ? 13 : 0;
                row |= (u32)c << (x * 4);
            }
            UI_OBJ[(OBJ_FRAME16 + t) * 8 + y] = row;
        }
    for (int y = 0; y < 8; y++) {
        u32 a = 0, b = 0;
        for (int x = 0; x < 8; x++) {
            a |= (u32)extraPixel(ART_DOT, x, y) << (x * 4);
            b |= (u32)extraPixel(ART_VIEW, x, y) << (x * 4);
        }
        UI_OBJ[OBJ_DOT * 8 + y] = a;
        UI_OBJ[OBJ_VIEW * 8 + y] = b;
    }
}

// ---- primitivas de desenho ----
static void uiClear(void) {
    for (int i = 0; i < 32 * 32; i++) { UI_TXT[i] = 0; UI_PNL[i] = 0; }
}
static void uiBox(int x, int y, int w, int h, int base, int pal) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int col = i == 0 ? 0 : (i == w - 1 ? 2 : 1);
            int row = j == 0 ? 0 : (j == h - 1 ? 2 : 1);
            UI_PNL[(y + j) * 32 + x + i] = (u16)((base + row * 3 + col) | (pal << 12));
        }
}
static void uiPanel(int x, int y, int w, int h) { uiBox(x, y, w, h, T_PANEL, P_UI); }
static void uiButton(int x, int y, int w, int h, bool sel) { uiBox(x, y, w, h, T_BUTTON, sel ? P_SEL : P_UI); }
static void uiSlot(int x, int y, bool sel) {
    for (int q = 0; q < 4; q++)
        UI_PNL[(y + (q >> 1)) * 32 + x + (q & 1)] = (u16)((T_SLOT + q) | ((sel ? P_SEL : P_UI) << 12));
}
static void uiText(int x, int y, const char *s, int pal) {
    for (; *s && x < 32; s++, x++) {
        int ch = (unsigned char)*s;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        UI_TXT[y * 32 + x] = (u16)((T_FONT + ch - 32) | (pal << 12));
    }
}
static void uiTextC(int x0, int w, int y, const char *s, int pal) { uiText(x0 + (w - (int)strlen(s)) / 2, y, s, pal); }
static void uiNum(int x, int y, int n, int pal) { char b[16]; sprintf(b, "%d", n); uiText(x, y, b, pal); }
static void uiBar(int x, int y, int w, int filled) {
    for (int i = 0; i < w; i++) UI_PNL[y * 32 + x + i] = (u16)(i < filled ? T_BAR + 1 : T_BAR);
}
static void nameFit(int id, char *buf, int max) {
    itemName(id, buf);
    if ((int)strlen(buf) > max) buf[max] = 0;
}
// nome curto para a lista de receitas: "PIC. FERRO", "BLOCO OURO", "TABUAS";
// se ainda nao couber, corta com ponto ("CARVAO VEGET.")
static void listName(int id, char *buf, int max) {
    if (isTool(id)) {
        static const char *t[] = {"PIC.", "MACH.", "PA", "ESP."};
        static const char *m[] = {"MADEIRA", "PEDRA", "FERRO", "OURO", "DIAMANTE"};
        sprintf(buf, "%s %s", t[toolType(id)], m[toolMat(id)]);
    } else {
        static const char *cut[] = {" de Carvalho", " de Trabalho"};
        itemName(id, buf);
        for (unsigned i = 0; i < sizeof cut / sizeof *cut; i++) {
            char *p = strstr(buf, cut[i]);
            if (p) *p = 0;
        }
        char *p = strstr(buf, " de ");
        if (p && (!strncmp(buf, "Bloco", 5) || !strncmp(buf, "Barra", 5))) memmove(p + 1, p + 4, strlen(p + 4) + 1);
    }
    if ((int)strlen(buf) > max) { buf[max - 1] = '.'; buf[max] = 0; }
}

// ---- sprites (OAM sombra) ----
static u16 oamShadow[128 * 4];
static int nSpr;
static void sprReset(void) {
    for (int i = 0; i < 128; i++) { oamShadow[i * 4] = 0x0200; oamShadow[i * 4 + 1] = 0; oamShadow[i * 4 + 2] = 0; }
    nSpr = 0;
}
static void sprPut(int x, int y, int tile, int pal, bool big) {
    if (nSpr >= 128) return;
    int i = nSpr++;
    oamShadow[i * 4]     = (u16)(y & 255);
    oamShadow[i * 4 + 1] = (u16)((x & 511) | (big ? 0x4000 : 0));
    oamShadow[i * 4 + 2] = (u16)(tile | (pal << 12));
}
static void sprIcon16(int x, int y, int id) { if (id) sprPut(x, y, OBJ_ICON16(id), iconBank(id), true); }
static void sprIcon8(int x, int y, int id) { if (id) sprPut(x, y, OBJ_ICON8(id), iconBank(id), false); }

// ---------------- Estado da UI ----------------
enum { TAB_WORLD, TAB_BAG, TAB_CRAFT, TAB_MENU };
static int tab = TAB_WORLD;
static bool uiDirty = true;
static int invPick = -1, invCur = 0;
static int craftCur = 0, craftScroll = 0;
static bool craftOnlyPossible = false;
static int craftList[64], craftCount = 0;
static bool hasTable = false, hasFurnace = false;
static char toastBuf[33];
static int toastTime = 0;
static int confirmNew = 0;

static void setToast(const char *t) { strncpy(toastBuf, t, 32); toastBuf[32] = 0; toastTime = 150; uiDirty = true; }

static void buildCraftList(void) {
    craftCount = 0;
    for (int i = 0; i < numRecipes; i++)
        if (!craftOnlyPossible || canCraft(i, hasTable, hasFurnace)) craftList[craftCount++] = i;
    if (craftCur >= craftCount) craftCur = craftCount ? craftCount - 1 : 0;
    if (craftScroll > craftCur) craftScroll = craftCur;
}

// ---- minimapa 96x96 (2 px por bloco) em 12x12 tiles ----
static int mapColorTop(int x, int z) {
    for (int y = WY - 1; y >= 0; y--) {
        int b = world[x][y][z];
        if (b == B_AIR) continue;
        bool low = y < 15;
        switch (b) {
        case B_GRASS:  return low ? MC_GRASS2 : MC_GRASS;
        case B_LEAVES: return MC_LEAF;
        case B_SAND:   return MC_SAND;
        case B_DIRT: case B_GRAVEL: return MC_DIRT;
        case B_LOG: case B_PLANKS: case B_CTABLE: return MC_WOOD;
        case B_GLASS:  return MC_GLASS;
        case B_IRON_BLOCK: case B_GOLD_BLOCK: case B_DIAMOND_BLOCK: return MC_METAL;
        default:       return low ? MC_STONE2 : MC_STONE;
        }
    }
    return MC_VOID;
}
static int mapColorSlice(int x, int y, int z) {   // modo caverna: fatia na altura do jogador
    int b = getBlock(x, y, z), up = getBlock(x, y + 1, z);
    if (b == B_AIR && up == B_AIR) return MC_DEEP;
    int t = b != B_AIR ? b : up;
    if (t >= B_COAL_ORE && t <= B_EMERALD_ORE)
        return t == B_REDSTONE_ORE ? MC_RED : (t == B_LAPIS_ORE || t == B_DIAMOND_ORE ? MC_BLUE : MC_ORE);
    if (t == B_BEDROCK) return MC_VOID;
    return t == B_DIRT || t == B_GRAVEL ? MC_DIRT : MC_STONE2;
}
static bool mapCave = false;

static void drawMinimapTiles(void) {
    int cy = (py >> 12);
    for (int ty = 0; ty < 12; ty++)
        for (int tx = 0; tx < 12; tx++)
            for (int y = 0; y < 8; y++) {
                u32 row = 0;
                for (int x = 0; x < 8; x++) {
                    int wx = (tx * 8 + x) / 2, wz = (ty * 8 + y) / 2;
                    int c = mapCave ? mapColorSlice(wx, cy, wz) : mapColorTop(wx, wz);
                    row |= (u32)c << (x * 4);
                }
                UI_TILES[(T_MAP + ty * 12 + tx) * 8 + y] = row;
            }
}

// ---- abas ----
static const char *tabNames[4] = {"MUNDO", "MOCHILA", "CRIAR", "MENU"};

static void drawTabs(void) {
    for (int t = 0; t < 4; t++) {
        uiButton(t * 8, 0, 8, 3, t == tab);
        uiTextC(t * 8, 8, 1, tabNames[t], t == tab ? P_UI : P_GREY);
    }
}

static void slotCount(int x, int y, const Slot *s) {
    if (!s->id) return;
    if (isTool(s->id)) {
        int pct = (toolMaxDur(s->id) - s->dmg) * 100 / toolMaxDur(s->id);
        uiNum(x, y, pct, pct > 50 ? P_GREEN : (pct > 20 ? P_GOLD : P_RED));
    } else if (s->count > 1) uiNum(x, y, s->count, P_UI);
}

static const char *tierName(int t) {
    static const char *n[] = {"", "MADEIRA", "PEDRA", "FERRO", "DIAMANTE"};
    return n[t < 5 ? t : 4];
}

static void drawWorldTab(void) {
    uiPanel(0, 3, 14, 14);
    uiText(1, 3, mapCave ? " CAVERNA " : " MAPA ", mapCave ? P_BLUE : P_GOLD);
    for (int ty = 0; ty < 12; ty++)
        for (int tx = 0; tx < 12; tx++)
            UI_PNL[(4 + ty) * 32 + 1 + tx] = (u16)((T_MAP + ty * 12 + tx) | (P_MAP << 12));

    uiPanel(14, 3, 18, 14);
    char name[40];
    const Slot *h = &inv[selSlot];
    if (h->id) {
        nameFit(h->id, name, 16); uiText(15, 4, name, P_GOLD);
        if (isTool(h->id)) {
            uiText(15, 5, "DUR", P_GREY);
            char b[16]; sprintf(b, "%d/%d", toolMaxDur(h->id) - h->dmg, toolMaxDur(h->id));
            uiText(19, 5, b, P_UI);
        } else { uiText(15, 5, "QTD", P_GREY); uiNum(19, 5, h->count, P_UI); }
    } else uiText(15, 4, "MAO VAZIA", P_GREY);

    uiText(15, 7, "MIRA", P_GREY);
    if (hasHit) {
        int b = world[hitX][hitY][hitZ];
        nameFit(b, name, 16); uiText(15, 8, name, P_UI);
        if (blockInfo[b].hard10 == 0xFFFF) uiText(15, 9, "INQUEBRAVEL", P_RED);
        else if (!canHarvest(b, h->id)) {
            char need[24]; sprintf(need, "PIC. %s+", tierName(blockInfo[b].minTier));
            uiText(15, 9, need, P_RED);
        } else uiText(15, 9, "PODE COLETAR", P_GREEN);
    } else uiText(15, 8, "-", P_GREY);

    uiBar(15, 11, 16, mineX >= 0 && mineNeed > 0 ? mineProg * 16 / mineNeed : 0);
    uiText(15, 13, "BANCADA", hasTable ? P_GREEN : P_GREY);
    uiText(23, 13, "FORNALHA", hasFurnace ? P_GREEN : P_GREY);
    char pos[40];
    sprintf(pos, "X%d Y%d Z%d", (int)(px >> 12), (int)(py >> 12), (int)(pz >> 12));
    uiText(15, 15, pos, P_GREY);
    uiText(15, 14, isUnderground() ? "SUBSOLO" : "SUPERFICIE", isUnderground() ? P_BLUE : P_GREEN);

    uiPanel(0, 17, 32, 6);
    for (int i = 0; i < 9; i++) {
        uiSlot(2 + i * 3, 18, i == selSlot);
        slotCount(2 + i * 3, 20, &inv[i]);
    }
    uiText(2, 21, "Y:QUEBRA A:POE B:PULO X:SLOT", P_GREY);
}

static void drawBagTab(void) {
    uiPanel(0, 3, 32, 14);
    for (int s = 0; s < INV_SIZE; s++) {
        int x = 2 + (s % 9) * 3, y = 4 + (s / 9) * 3;
        uiSlot(x, y, s == invPick || (invPick < 0 && s == invCur));
        slotCount(x, y + 2, &inv[s]);
    }
    uiText(29, 4, "1-9", P_GOLD);

    uiPanel(0, 17, 32, 7);
    const Slot *s = &inv[invPick >= 0 ? invPick : invCur];
    char name[40], b[40];
    if (s->id) {
        nameFit(s->id, name, 28); uiText(2, 18, name, P_GOLD);
        if (isTool(s->id)) {
            static const char *types[] = {"PICARETA", "MACHADO", "PA", "ESPADA"};
            sprintf(b, "%s  DUR %d/%d", types[toolType(s->id)], toolMaxDur(s->id) - s->dmg, toolMaxDur(s->id));
            uiText(2, 19, b, P_UI);
        } else if (isPlaceable(s->id)) {
            sprintf(b, "BLOCO  X%d  DUREZA %d.%d", s->count, blockInfo[s->id].hard10 / 10, blockInfo[s->id].hard10 % 10);
            uiText(2, 19, b, P_UI);
        } else { sprintf(b, "ITEM  X%d", s->count); uiText(2, 19, b, P_UI); }
    } else uiText(2, 18, "SLOT VAZIO", P_GREY);
    uiText(2, 21, invPick >= 0 ? "TOQUE NO DESTINO PARA TROCAR" : "TOQUE PARA PEGAR UM ITEM", P_GREY);
}

static void drawCraftTab(void) {
    buildCraftList();
    uiButton(0, 3, 9, 3, !craftOnlyPossible);  uiTextC(0, 9, 4, "TODAS", P_UI);
    uiButton(9, 3, 9, 3, craftOnlyPossible);   uiTextC(9, 9, 4, "POSSIVEIS", P_UI);
    uiButton(18, 3, 7, 3, false);              uiTextC(18, 7, 4, "^", P_UI);
    uiButton(25, 3, 7, 3, false);              uiTextC(25, 7, 4, "V", P_UI);

    // lista: uma linha de texto por receita e uma linha livre entre elas;
    // icone 16x16 centrado na linha, quantidade ou estacao (icone 8x8) a direita
    uiPanel(0, 6, 18, 18);
    char name[40];
    for (int i = 0; i < 8 && craftScroll + i < craftCount; i++) {
        int ri = craftList[craftScroll + i];
        const Recipe *r = &recipes[ri];
        int y = 7 + i * 2;
        bool can = canCraft(ri, hasTable, hasFurnace);
        bool sel = craftScroll + i == craftCur;
        if (sel)
            for (int x = 1; x < 17; x++) UI_PNL[y * 32 + x] = (u16)((T_BUTTON + 4) | (P_SEL << 12));
        listName(r->out, name, r->outN > 1 ? 11 : 13);
        uiText(3, y, name, can || sel ? P_UI : P_GREY);
        if (r->outN > 1) { char b[8]; sprintf(b, "X%d", r->outN); uiText(15, y, b, sel ? P_GOLD : P_GREY); }
    }
    if (!craftCount) uiText(2, 8, "NADA POR ENQUANTO", P_GREY);

    uiPanel(18, 6, 14, 18);
    if (craftCount) {
        int ri = craftList[craftCur];
        const Recipe *r = &recipes[ri];
        itemName(r->out, name);
        // quebra o nome em ate 2 linhas de 9
        char l1[16] = "", l2[16] = "";
        int cut = (int)strlen(name);
        if (cut > 9) { cut = 9; while (cut > 0 && name[cut] != ' ') cut--; if (!cut) cut = 9; }
        strncpy(l1, name, (size_t)cut); l1[cut] = 0;
        strncpy(l2, name + cut + (name[cut] == ' '), 9); l2[9] = 0;
        uiText(22, 7, l1, P_GOLD); uiText(22, 8, l2, P_GOLD);
        char b[24];
        sprintf(b, "RENDE X%d", r->outN); uiText(19, 10, b, P_UI);
        if (r->station == ST_NONE) uiText(19, 11, "INVENTARIO", P_GREY);
        else if (r->station == ST_TABLE) uiText(19, 11, "BANCADA", hasTable ? P_GREEN : P_RED);
        else uiText(19, 11, "FORNALHA", hasFurnace ? P_GREEN : P_RED);
        uiText(19, 13, "PRECISA", P_GREY);
        for (int i = 0; i < r->n; i++) {      // ingrediente: nome e "tem/precisa", linha livre entre eles
            int have = countItem(r->inId[i]);
            listName(r->inId[i], name, 10);
            uiText(21, 14 + i * 3, name, P_UI);
            sprintf(b, "%d/%d", have > 99 ? 99 : have, r->inN[i]);
            uiText(21, 15 + i * 3, b, have >= r->inN[i] ? P_GREEN : P_RED);
        }
        if (r->station == ST_FURNACE) {
            sprintf(b, "COMB %d.%d", fuelHalf / 2, (fuelHalf & 1) * 5);
            uiText(19, 18, b, P_GREY);
        }
        bool can = canCraft(ri, hasTable, hasFurnace);
        uiButton(19, 20, 12, 3, can);
        uiTextC(19, 12, 21, "CRIAR", can ? P_UI : P_GREY);
    }
}

static void drawMenuTab(void) {
    uiButton(1, 4, 14, 3, false);  uiTextC(1, 14, 5, "SALVAR", P_UI);
    uiButton(1, 8, 14, 3, false);  uiTextC(1, 14, 9, "CARREGAR", P_UI);
    uiButton(1, 12, 14, 3, confirmNew > 0); uiTextC(1, 14, 13, confirmNew ? "CONFIRMAR?" : "NOVO MUNDO", confirmNew ? P_UI : P_UI);

    uiPanel(16, 3, 16, 14);
    uiText(18, 4, "OPCOES", P_GOLD);
    uiText(18, 6, "SENSIBILIDADE", P_GREY);
    uiText(18, 7, "<", P_GOLD); uiNum(22, 7, optSens, P_UI); uiText(26, 7, ">", P_GOLD);
    uiText(18, 9, "INVERTER Y", P_GREY);
    uiButton(18, 10, 6, 3, optInvertY); uiTextC(18, 6, 11, optInvertY ? "SIM" : "NAO", P_UI);
    uiText(18, 14, "DISTANCIA", P_GREY);
    uiText(18, 15, "<", P_GOLD); uiNum(22, 15, renderR, P_UI); uiText(26, 15, ">", P_GOLD);

    uiPanel(0, 17, 32, 7);
    uiText(2, 18, "BLOCODS  FASE 2", P_GOLD);
    char b[32]; sprintf(b, "SEMENTE %u", (unsigned)seed); uiText(2, 19, b, P_GREY);
    uiText(2, 21, "START ABRE ESTE MENU", P_GREY);
}

static void drawUI(void) {
    if (uiDirty) {
        uiClear();
        drawTabs();
        switch (tab) {
        case TAB_WORLD: drawWorldTab(); break;
        case TAB_BAG:   drawBagTab(); break;
        case TAB_CRAFT: drawCraftTab(); break;
        case TAB_MENU:  drawMenuTab(); break;
        }
        if (toastTime) uiTextC(0, 32, 23, toastBuf, P_GOLD);
        uiDirty = false;
    }
    // sprites mudam todo frame (icones e marcadores)
    sprReset();
    if (tab == TAB_WORLD) {
        for (int i = 0; i < 9; i++) sprIcon16((2 + i * 3) * 8, 18 * 8, inv[i].id);
        int mx = 8 + (int)((px * 2) >> 12), mz = 32 + (int)((pz * 2) >> 12);
        sprPut(mx - 4 + ((fwdX * 5) >> 12), mz - 4 + ((fwdZ * 5) >> 12), OBJ_VIEW, 0, false);
        sprPut(mx - 4, mz - 4, OBJ_DOT, 0, false);
    } else if (tab == TAB_BAG) {
        for (int s = 0; s < INV_SIZE; s++) sprIcon16((2 + (s % 9) * 3) * 8, (4 + (s / 9) * 3) * 8, inv[s].id);
    } else if (tab == TAB_CRAFT) {
        for (int i = 0; i < 8 && craftScroll + i < craftCount; i++) {
            const Recipe *r = &recipes[craftList[craftScroll + i]];
            int y = (7 + i * 2) * 8;
            sprIcon16(8, y - 4, r->out);
            if (r->station != ST_NONE) sprIcon8(16 * 8 + 4, y, r->station == ST_TABLE ? B_CTABLE : B_FURNACE);
        }
        if (craftCount) {
            const Recipe *r = &recipes[craftList[craftCur]];
            sprIcon16(19 * 8, 7 * 8, r->out);
            for (int i = 0; i < r->n; i++) sprIcon16(19 * 8 - 4, (14 + i * 3) * 8 + 2, r->inId[i]);
        }
    }
}

// ---------------- Save em arquivo (FAT) ----------------
static bool fatOk = false;
#define SAVE_MAGIC 0x31534442u   // "BDS1"

static FILE *openSave(const char *mode) {
    FILE *f = fopen("blocods.sav", mode);
    if (!f) f = fopen("fat:/blocods.sav", mode);
    if (!f) f = fopen("sd:/blocods.sav", mode);
    return f;
}

static bool saveGame(void) {
    if (!fatOk) return false;
    FILE *f = openSave("wb");
    if (!f) return false;
    u32 hdr[12] = {SAVE_MAGIC, seed, (u32)px, (u32)py, (u32)pz, (u32)yaw, (u32)pitch,
                   (u32)selSlot, (u32)fuelHalf, (u32)optSens, (u32)optInvertY, (u32)renderR};
    bool ok = fwrite(hdr, sizeof hdr, 1, f) == 1 &&
              fwrite(inv, sizeof inv, 1, f) == 1 &&
              fwrite(heightMap, sizeof heightMap, 1, f) == 1 &&
              fwrite(world, sizeof world, 1, f) == 1;
    fclose(f);
    return ok;
}

static bool loadGame(void) {
    if (!fatOk) return false;
    FILE *f = openSave("rb");
    if (!f) return false;
    u32 hdr[12];
    bool ok = fread(hdr, sizeof hdr, 1, f) == 1 && hdr[0] == SAVE_MAGIC;
    if (ok) ok = fread(inv, sizeof inv, 1, f) == 1 && fread(heightMap, sizeof heightMap, 1, f) == 1 &&
                 fread(world, sizeof world, 1, f) == 1;
    fclose(f);
    if (!ok) return false;
    seed = hdr[1]; px = (fx)hdr[2]; py = (fx)hdr[3]; pz = (fx)hdr[4];
    yaw = (int)hdr[5]; pitch = (int)hdr[6]; selSlot = (int)hdr[7] % 9; fuelHalf = (int)hdr[8];
    optSens = (int)hdr[9]; optInvertY = hdr[10] != 0; renderR = (int)hdr[11];
    if (optSens < 1 || optSens > 3) optSens = 2;
    if (renderR < 6 || renderR > 8) renderR = 8;
    vy = 0;
    computeAllFaces();
    mapDirty = true;
    return true;
}

// ---------------- Entrada por toque ----------------
static int lastTX, lastTY;
static bool lookDrag = false;

static void touchTabs(int col) { tab = col / 8; invPick = -1; confirmNew = 0; uiDirty = true; }

static void touchWorld(int col, int row, bool down, touchPosition *tp) {
    if (down && row >= 18 && row <= 20 && col >= 2 && col < 29 && (col - 2) % 3 < 2) {
        selSlot = (col - 2) / 3; stopMining(); uiDirty = true; lookDrag = false; return;
    }
    if (down && row >= 4 && row < 16 && col >= 1 && col < 13 && tp->px < 8 * 13) {
        // toque no minimapa alterna superficie / caverna
        mapCave = !mapCave; mapDirty = true; uiDirty = true; lookDrag = false; return;
    }
    if (down) { lookDrag = row >= 3; lastTX = tp->px; lastTY = tp->py; return; }
    if (lookDrag) {
        yaw += (tp->px - lastTX) * 32 * optSens;
        pitch += (tp->py - lastTY) * 32 * optSens * (optInvertY ? -1 : 1);
        lastTX = tp->px; lastTY = tp->py;
    }
}

static void touchBag(int col, int row) {
    if (row < 4 || row > 15 || col < 2 || col > 28) return;
    int r = (row - 4) / 3, c = (col - 2) / 3;
    if ((row - 4) % 3 == 2 || (col - 2) % 3 == 2 || r > 3 || c > 8) return;
    int s = r * 9 + c;
    if (invPick < 0) { if (inv[s].id) invPick = s; invCur = s; }
    else {
        Slot a = inv[invPick], *b = &inv[s];
        if (a.id == b->id && !isTool(a.id) && invPick != s) {
            int k = 64 - b->count; if (k > a.count) k = a.count;
            b->count += k; inv[invPick].count -= k;
            if (!inv[invPick].count) inv[invPick].id = 0;
        } else { inv[invPick] = *b; *b = a; }
        invPick = -1; invCur = s;
    }
    uiDirty = true;
}

static void touchCraft(int col, int row) {
    buildCraftList();
    if (row >= 3 && row <= 5) {
        if (col < 9) craftOnlyPossible = false;
        else if (col < 18) craftOnlyPossible = true;
        else if (col < 25) { craftScroll = craftScroll > 0 ? craftScroll - 1 : 0; if (craftCur > craftScroll + 7) craftCur = craftScroll + 7; }
        else if (craftScroll + 8 < craftCount) { craftScroll++; if (craftCur < craftScroll) craftCur = craftScroll; }
        craftCur = craftCur < craftScroll ? craftScroll : craftCur;
        uiDirty = true;
        return;
    }
    if (col < 18 && row >= 7 && row < 23) {
        int i = craftScroll + (row - 7) / 2;
        if (i < craftCount) {
            if (i == craftCur && craft(craftList[i], hasTable, hasFurnace)) setToast("CRIADO!");
            craftCur = i;
            uiDirty = true;
        }
        return;
    }
    if (col >= 19 && col < 31 && row >= 20 && row <= 22 && craftCount) {
        if (craft(craftList[craftCur], hasTable, hasFurnace)) setToast("CRIADO!");
        else setToast("FALTA MATERIAL OU ESTACAO");
        uiDirty = true;
    }
}

static void touchMenu(int col, int row) {
    if (col >= 1 && col < 15) {
        if (row >= 4 && row <= 6) { setToast(saveGame() ? "MUNDO SALVO!" : "ERRO AO SALVAR (SEM CARTAO?)"); confirmNew = 0; }
        else if (row >= 8 && row <= 10) { setToast(loadGame() ? "MUNDO CARREGADO!" : "NENHUM SAVE ENCONTRADO"); confirmNew = 0; }
        else if (row >= 12 && row <= 14) {
            if (!confirmNew) { confirmNew = 1; setToast("TOQUE DE NOVO PARA CONFIRMAR"); }
            else {
                seed = seed * 1103515245u + 12345u;
                invClear(); generateWorld(); spawnPlayer(); selSlot = 0; confirmNew = 0;
                tab = TAB_WORLD;
                setToast("NOVO MUNDO GERADO!");
            }
        }
        uiDirty = true;
        return;
    }
    if (row == 7 && col >= 17 && col <= 19 && optSens > 1) optSens--;
    if (row == 7 && col >= 25 && col <= 27 && optSens < 3) optSens++;
    if (row >= 10 && row <= 12 && col >= 18 && col < 24) optInvertY = !optInvertY;
    if (row == 15 && col >= 17 && col <= 19 && renderR > 6) renderR--;
    if (row == 15 && col >= 25 && col <= 27 && renderR < 8) renderR++;
    uiDirty = true;
}

static void handleTouch(u32 held, u32 down) {
    if (!(held & KEY_TOUCH)) { lookDrag = false; return; }
    touchPosition tp;
    touchRead(&tp);
    int col = tp.px / 8, row = tp.py / 8;
    bool isDown = down & KEY_TOUCH;
    if (isDown && row < 3) { touchTabs(col); return; }
    switch (tab) {
    case TAB_WORLD: touchWorld(col, row, isDown, &tp); break;
    case TAB_BAG:   if (isDown) touchBag(col, row); break;
    case TAB_CRAFT: if (isDown) touchCraft(col, row); break;
    case TAB_MENU:  if (isDown) touchMenu(col, row); break;
    }
}

// ---------------- Principal ----------------
static u32 frame = 0;

void gameInit(void) {
    videoSetMode(MODE_0_3D);
    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_ACTIVE | DISPLAY_SPR_1D);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE);
    REG_BG0CNT_SUB = (u16)(0 | (0 << 2) | (28 << 8));   // texto: prioridade 0
    REG_BG1CNT_SUB = (u16)(1 | (0 << 2) | (29 << 8));   // paineis: prioridade 1
    uiPalettes();
    uiTiles();
    uiClear();

    glInit();
    glEnable(GL_ANTIALIAS);
    glClearColor(SKY_R, SKY_G, SKY_B, 31);
    glClearPolyID(63);
    glClearDepth(GL_MAX_DEPTH);
    glViewport(0, 0, 255, 191);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(65, 256.0f / 192.0f, 0.08f, 14.0f);

    fatOk = fatInitDefault();
    initRecipes();
    invClear();
    if (loadGame()) setToast("SAVE CARREGADO");
    else { generateWorld(); spawnPlayer(); }
}

void gameFrame(void) {
    // OAM da tela de baixo e copiada logo apos o VBlank
    for (int i = 0; i < 128 * 4; i++) OAM_SUB[i] = oamShadow[i];

    scanKeys();
    u32 held = keysHeld(), down = keysDown();
    handleTouch(held, down);

    if (held & KEY_L) yaw -= 220;
    if (held & KEY_R) yaw += 220;
    if (down & KEY_X) { selSlot = (selSlot + 1) % 9; uiDirty = true; stopMining(); }
    if (down & KEY_SELECT) { tab = (tab + 1) % 4; invPick = -1; confirmNew = 0; uiDirty = true; }
    if (down & KEY_START) { tab = TAB_MENU; confirmNew = 0; uiDirty = true; }

    yaw &= 0x7FFF;
    if (pitch > 7000) pitch = 7000;
    if (pitch < -7000) pitch = -7000;
    updateCamera();

    fx mx = 0, mz = 0;
    fx sy = sinLerp((s16)yaw), cy = cosLerp((s16)yaw);
    if (held & KEY_UP)    { mx += sy; mz -= cy; }
    if (held & KEY_DOWN)  { mx -= sy; mz += cy; }
    if (held & KEY_RIGHT) { mx += cy; mz += sy; }
    if (held & KEY_LEFT)  { mx -= cy; mz -= sy; }
    if ((down & KEY_B) && onGround) vy = JUMP_V;
    movePlayer(fmul(mx, SPEED), fmul(mz, SPEED));
    int oldHit = hasHit ? hitX * 10000 + hitY * 100 + hitZ : -1;
    raycast();
    int newHit = hasHit ? hitX * 10000 + hitY * 100 + hitZ : -1;
    if (oldHit != newHit && tab == TAB_WORLD) uiDirty = true;

    // quebrar segurando Y (tempo da wiki)
    if (mineCooldown) mineCooldown--;
    if ((held & KEY_Y) && hasHit && !mineCooldown) {
        if (mineX != hitX || mineY != hitY || mineZ != hitZ) {
            mineX = hitX; mineY = hitY; mineZ = hitZ; mineProg = 0;
            mineNeed = breakFrames(world[hitX][hitY][hitZ], inv[selSlot].id);
        }
        if (mineNeed > 0 && ++mineProg >= mineNeed) {
            harvest(world[mineX][mineY][mineZ], selSlot);
            setBlock(mineX, mineY, mineZ, B_AIR);
            stopMining();
            mineCooldown = 18;   // 6 ticks entre blocos (Java)
        }
        if (tab == TAB_WORLD) uiDirty = true;
    } else if (mineX >= 0 && (!(held & KEY_Y) || !hasHit)) {
        stopMining();
        uiDirty = true;
    }

    // colocar com A
    if ((down & KEY_A) && hasHit && inBounds(prevX, prevY, prevZ)) {
        Slot *s = &inv[selSlot];
        if (isPlaceable(s->id) && world[prevX][prevY][prevZ] == B_AIR) {
            setBlock(prevX, prevY, prevZ, s->id);
            if (collides(px, py, pz)) setBlock(prevX, prevY, prevZ, B_AIR);
            else if (!--s->count) s->id = 0;
            uiDirty = true;
        }
    }

    if ((frame & 15) == 0) {
        bool t = nearBlock(B_CTABLE), f = nearBlock(B_FURNACE);
        if (t != hasTable || f != hasFurnace) { hasTable = t; hasFurnace = f; uiDirty = true; }
        bool cave = isUnderground();
        static bool lastCave = false;
        if (cave != lastCave) { lastCave = cave; mapCave = cave; mapDirty = true; uiDirty = true; }
        if (tab == TAB_WORLD) uiDirty = true;      // coordenadas
    }
    if (mapCave && (frame & 31) == 0) mapDirty = true;   // fatia acompanha a altura
    if (mapDirty && tab == TAB_WORLD) { drawMinimapTiles(); mapDirty = false; }
    if (toastTime && !--toastTime) uiDirty = true;

    renderWorld();
    renderCrosshair();
    glFlush(0);
    drawUI();
    frame++;
}

#ifndef HOST_PREVIEW
int main(void) {
    gameInit();
    while (1) {
        swiWaitForVBlank();
        gameFrame();
    }
}
#endif
