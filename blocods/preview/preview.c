// Gera prints do BlocoDS rodando o codigo real do jogo no PC.
// Os cenarios posicionam o jogador e dao itens; o resto (mineracao, crafting,
// render 3D, interface da tela de baixo) e o jogo funcionando com botoes e toques simulados.
#include <math.h>
#include <stdlib.h>
#include "../source/main.c"

extern u32 h_held, h_prev;
extern int h_tx, h_ty;
void h_dump(const char *name);

static void step(u32 keys, int frames) {
    while (frames-- > 0) { h_prev = h_held; h_held = keys; gameFrame(); }
}
static void press(u32 keys) { step(keys, 1); step(0, 4); }
static void touch(int col, int row) { h_tx = col * 8 + 4; h_ty = row * 8 + 4; step(KEY_TOUCH, 1); step(0, 4); }

static void lookAt(float tx, float ty, float tz) {
    float ex = px / 4096.f, ey = (py + P_EYE) / 4096.f, ez = pz / 4096.f;
    float dx = tx - ex, dy = ty - ey, dz = tz - ez, hz = sqrtf(dx * dx + dz * dz);
    yaw = (int)lrintf(atan2f(dx, -dz) * 16384.f / (float)M_PI) & 0x7FFF;
    pitch = (int)lrintf(-atan2f(dy, hz) * 16384.f / (float)M_PI);
}
static void placeAt(int x, int y, int z) { px = FX(x) + FXF(0.5); py = FX(y); pz = FX(z) + FXF(0.5); vy = 0; }

int main(void) {
    gameInit();
    step(0, 5);

    // 1) MUNDO: socando uma arvore
    int best = 1 << 30, tx = 0, tz = 0, ty = 0;
    for (int x = 3; x < WX - 3; x++)
        for (int z = 3; z < WZ - 3; z++) {
            int h = heightMap[x][z];
            if (world[x][h + 1][z] != B_LOG) continue;
            int d = abs(x - (int)(px >> 12)) + abs(z - (int)(pz >> 12));
            if (d < best && world[x][h][z + 3] == B_GRASS && world[x][h + 1][z + 3] == B_AIR && world[x][h + 2][z + 3] == B_AIR) {
                best = d; tx = x; tz = z; ty = h;
            }
        }
    placeAt(tx, ty + 1, tz + 3);
    step(0, 10);
    lookAt(tx + 0.5f, ty + 2.5f, tz + 0.5f);
    addItem(B_LOG, 3); addItem(B_DIRT, 12); addItem(TOOL(M_WOOD, T_AXE), 1);
    inv[2].dmg = 20;
    step(KEY_Y, 100);
    h_dump("ds1_mundo");
    step(KEY_Y, 120);
    step(0, 5);

    // 2) CRIAR: bancada perto, tabuas -> gravetos -> picareta de pedra
    addItem(B_COBBLE, 12);
    setBlock(tx + 1, ty + 1, tz + 3, B_CTABLE);
    step(0, 20);
    touch(17, 1);                        // aba CRIAR
    touch(4, 7); touch(4, 7); touch(4, 7);    // seleciona e cria tabuas (2x)
    touch(4, 9); touch(4, 9);            // gravetos
    touch(28, 4); touch(28, 4); touch(28, 4); touch(28, 4);   // rola a lista
    for (int i = 0; i < craftCount; i++)
        if (recipes[craftList[i]].out == TOOL(M_STONE, T_PICK)) { craftCur = i; break; }
    uiDirty = true;
    touch(25, 21);                       // botao CRIAR
    step(0, 5);
    h_dump("ds2_criar");

    // 3) caverna com minerios, picareta de ferro na mao
    int bestScore = -1, cx = 0, cy = 0, cz = 0;
    for (int x = 3; x < WX - 3; x++)
        for (int y = 3; y < 12; y++)
            for (int z = 3; z < WZ - 3; z++) {
                if (world[x][y][z] || world[x][y + 1][z] || !world[x][y - 1][z]) continue;
                int sc = 0;
                for (int dx = -5; dx <= 5; dx++)
                    for (int dy = -2; dy <= 3; dy++)
                        for (int dz = -5; dz <= 5; dz++) {
                            int b = getBlock(x + dx, y + dy, z + dz);
                            if (b == B_AIR) sc += 2;
                            else if (b >= B_COAL_ORE && b <= B_EMERALD_ORE && faces[x + dx][y + dy][z + dz])
                                sc += b == B_DIAMOND_ORE ? 40 : 12;
                        }
                if (sc > bestScore) { bestScore = sc; cx = x; cy = y; cz = z; }
            }
    touch(1, 1);                         // aba MUNDO
    placeAt(cx, cy, cz);
    inv[1].id = TOOL(M_IRON, T_PICK); inv[1].count = 1; inv[1].dmg = 37;
    selSlot = 1;
    step(0, 10);
    int ov = -1, bestYaw = yaw, bestPitch = pitch;
    for (int dx = -5; dx <= 5; dx++)
        for (int dy = -2; dy <= 3; dy++)
            for (int dz = -5; dz <= 5; dz++) {
                int ox = cx + dx, oy = cy + dy, oz = cz + dz;
                int b = getBlock(ox, oy, oz);
                if (b < B_COAL_ORE || b > B_EMERALD_ORE || !faces[ox][oy][oz]) continue;
                lookAt(ox + 0.5f, oy + 0.5f, oz + 0.5f);
                updateCamera(); raycast();
                if (!hasHit || hitX != ox || hitY != oy || hitZ != oz) continue;
                int v = (b == B_DIAMOND_ORE ? 100 : (b - B_COAL_ORE) * 10) + (abs(dx) + abs(dz));
                if (v > ov) { ov = v; bestYaw = yaw; bestPitch = pitch; }
            }
    yaw = bestYaw; pitch = bestPitch;
    step(0, 40);
    step(KEY_Y, 8);
    h_dump("ds3_caverna");
    step(KEY_Y, 40);
    step(0, 5);

    // 4) MOCHILA
    addItem(I_DIAMOND, 3); addItem(I_RAW_IRON, 7); addItem(I_COAL, 21); addItem(I_REDSTONE, 9);
    addItem(I_LAPIS, 6); addItem(B_GLASS, 8); addItem(TOOL(M_DIAMOND, T_SWORD), 1); addItem(B_FURNACE, 1);
    addItem(I_GOLD, 4); addItem(I_EMERALD, 2); addItem(I_APPLE, 1); addItem(B_IRON_BLOCK, 2);
    touch(12, 1);                        // aba MOCHILA
    touch(2 + 3 * 4, 7);                 // pega um item da 2a linha
    h_dump("ds4_mochila");

    // 5) MENU
    touch(28, 1);
    touch(26, 7);                        // sensibilidade +
    h_dump("ds5_menu");
    touch(5, 5);                         // salvar
    h_dump("ds5_menu");

    // 6) mudas: planta numa campina aberta e espera virar arvore
    touch(1, 1);                         // aba MUNDO
    int mx0 = 0, mz0 = 0;
    for (int x = 6; x < WX - 6 && !mx0; x++)
        for (int z = 10; z < WZ - 6; z++) {
            int h = heightMap[x][z];
            bool ok = world[x][h][z] == B_GRASS;
            for (int dx = -2; dx <= 2 && ok; dx++)
                for (int dz = -6; dz <= 2 && ok; dz++) {
                    int hh = heightMap[x + dx][z + dz];
                    if (hh > h || hh < h - 1) ok = false;
                    for (int y = h + 1; y < WY; y++) if (world[x + dx][y][z + dz]) ok = false;
                }
            if (ok) { mx0 = x; mz0 = z; break; }
        }
    int my0 = heightMap[mx0][mz0];
    placeAt(mx0, my0 + 1, mz0 - 4);
    inv[3].id = B_SAPLING; inv[3].count = 3; inv[3].dmg = 0;
    selSlot = 3;
    step(0, 160);                        // ceu volta ao azul e o aviso some
    lookAt(mx0 + 0.5f, my0 + 1.0f, mz0 + 0.5f);
    step(0, 3);
    int sx6 = prevX, sy6 = prevY, sz6 = prevZ;
    press(KEY_A);                        // planta
    lookAt(sx6 + 0.5f, sy6 + 0.6f, sz6 + 0.5f);
    step(0, 3);
    h_dump("ds6_muda");
    int frames = 0;                      // tempo de jogo ate crescer (~2 min em media)
    while (world[sx6][sy6][sz6] == B_SAPLING && frames < 60 * 60 * 20) { randomTicks(); frames++; }
    printf("muda cresceu em %d frames (%d s)\n", frames, frames / 60);
    pz -= FX(2);
    step(0, 10);
    lookAt(sx6 + 0.5f, sy6 + 3.0f, sz6 + 0.5f);
    step(0, 3);
    h_dump("ds7_arvore");
    return 0;
}
