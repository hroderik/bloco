// BlocoCore - regras e dados do jogo (ver core.h)
#include "core.h"
#include <string.h>
#include <stdio.h>

// name, ab, top, side, det, detKind, hard10, tool, minTier, drop, dmin, dmax
const BlockInfo blockInfo[NUM_BLOCKS] = {
    [B_AIR]          = {"Ar",                     "  ", {0,0,0},    {0,0,0},    {0,0,0},    DET_NONE, 0,      TOOL_NONE,   0, 0, 0, 0},
    [B_GRASS]        = {"Bloco de Grama",         "Gr", {9,24,6},   {14,10,5},  {9,24,6},   DET_BAND, 6,      TOOL_SHOVEL, 0, B_DIRT, 1, 1},
    [B_DIRT]         = {"Terra",                  "Te", {14,10,5},  {14,10,5},  {0,0,0},    DET_NONE, 5,      TOOL_SHOVEL, 0, B_DIRT, 1, 1},
    [B_STONE]        = {"Pedra",                  "Pd", {16,16,17}, {15,15,16}, {0,0,0},    DET_NONE, 15,     TOOL_PICK,   1, B_COBBLE, 1, 1},
    [B_COBBLE]       = {"Pedregulho",             "Pg", {13,13,13}, {12,12,12}, {8,8,8},    DET_SPOT, 20,     TOOL_PICK,   1, B_COBBLE, 1, 1},
    [B_SAND]         = {"Areia",                  "Ar", {29,27,17}, {27,25,15}, {0,0,0},    DET_NONE, 5,      TOOL_SHOVEL, 0, B_SAND, 1, 1},
    [B_GRAVEL]       = {"Cascalho",               "Cs", {17,15,14}, {16,14,13}, {10,9,9},   DET_SPOT, 6,      TOOL_SHOVEL, 0, B_GRAVEL, 1, 1},
    [B_LOG]          = {"Tronco de Carvalho",     "Tr", {22,17,9},  {13,9,4},   {16,11,5},  DET_NONE, 20,     TOOL_AXE,    0, B_LOG, 1, 1},
    [B_LEAVES]       = {"Folhas de Carvalho",     "Fo", {6,18,5},   {5,16,4},   {0,0,0},    DET_NONE, 2,      TOOL_NONE,   0, 0, 0, 0},
    [B_PLANKS]       = {"Tabuas de Carvalho",     "Tb", {24,18,10}, {22,16,9},  {0,0,0},    DET_NONE, 20,     TOOL_AXE,    0, B_PLANKS, 1, 1},
    [B_CTABLE]       = {"Bancada de Trabalho",    "Bc", {24,18,10}, {20,13,7},  {12,8,4},   DET_SPOT, 25,     TOOL_AXE,    0, B_CTABLE, 1, 1},
    [B_FURNACE]      = {"Fornalha",               "Fn", {14,14,15}, {12,12,13}, {3,3,3},    DET_SPOT, 35,     TOOL_PICK,   1, B_FURNACE, 1, 1},
    [B_GLASS]        = {"Vidro",                  "Vi", {24,29,31}, {22,27,30}, {0,0,0},    DET_NONE, 3,      TOOL_NONE,   0, 0, 0, 0},
    [B_COAL_ORE]     = {"Minerio de Carvao",      "Mc", {16,16,17}, {15,15,16}, {2,2,2},    DET_SPOT, 30,     TOOL_PICK,   1, I_COAL, 1, 1},
    [B_IRON_ORE]     = {"Minerio de Ferro",       "Mf", {16,16,17}, {15,15,16}, {27,20,15}, DET_SPOT, 30,     TOOL_PICK,   2, I_RAW_IRON, 1, 1},
    [B_GOLD_ORE]     = {"Minerio de Ouro",        "Mo", {16,16,17}, {15,15,16}, {31,27,6},  DET_SPOT, 30,     TOOL_PICK,   3, I_RAW_GOLD, 1, 1},
    [B_REDSTONE_ORE] = {"Minerio de Redstone",    "Mr", {16,16,17}, {15,15,16}, {28,3,3},   DET_SPOT, 30,     TOOL_PICK,   3, I_REDSTONE, 4, 5},
    [B_LAPIS_ORE]    = {"Minerio de Lapis-Lazuli","Ml", {16,16,17}, {15,15,16}, {5,9,26},   DET_SPOT, 30,     TOOL_PICK,   2, I_LAPIS, 4, 9},
    [B_DIAMOND_ORE]  = {"Minerio de Diamante",    "Md", {16,16,17}, {15,15,16}, {12,29,28}, DET_SPOT, 30,     TOOL_PICK,   3, I_DIAMOND, 1, 1},
    [B_EMERALD_ORE]  = {"Minerio de Esmeralda",   "Me", {16,16,17}, {15,15,16}, {6,28,10},  DET_SPOT, 30,     TOOL_PICK,   3, I_EMERALD, 1, 1},
    [B_IRON_BLOCK]   = {"Bloco de Ferro",         "Bf", {27,27,28}, {24,24,25}, {0,0,0},    DET_NONE, 50,     TOOL_PICK,   2, B_IRON_BLOCK, 1, 1},
    [B_GOLD_BLOCK]   = {"Bloco de Ouro",          "Bo", {31,26,8},  {29,23,6},  {0,0,0},    DET_NONE, 30,     TOOL_PICK,   3, B_GOLD_BLOCK, 1, 1},
    [B_DIAMOND_BLOCK]= {"Bloco de Diamante",      "Bd", {14,29,28}, {12,26,25}, {0,0,0},    DET_NONE, 50,     TOOL_PICK,   3, B_DIAMOND_BLOCK, 1, 1},
    [B_BEDROCK]      = {"Rocha-Matriz",           "Rm", {6,6,6},    {5,5,5},    {12,12,12}, DET_SPOT, 0xFFFF, TOOL_NONE,   0, 0, 0, 0},
};

static const struct { const char *name; const char *ab; } simpleItems[] = {
    {"Graveto", "Gv"}, {"Carvao", "Cv"}, {"Carvao Vegetal", "Cg"}, {"Ferro Bruto", "Fb"},
    {"Ouro Bruto", "Ob"}, {"Barra de Ferro", "Lf"}, {"Barra de Ouro", "Lo"}, {"Diamante", "Di"},
    {"Esmeralda", "Es"}, {"Po de Redstone", "Rs"}, {"Lapis-Lazuli", "La"}, {"Pederneira", "Pe"},
    {"Maca", "Ma"},
};

// Wiki: durabilidade, multiplicador de velocidade e tier de coleta por material
static const char *matName[NUM_MATS]   = {"Madeira", "Pedra", "Ferro", "Ouro", "Diamante"};
static const char  matChar[NUM_MATS]   = {'m', 'p', 'f', 'o', 'd'};
static const int   matDur[NUM_MATS]    = {59, 131, 250, 32, 1561};
static const int   matSpeed[NUM_MATS]  = {2, 4, 6, 12, 8};
static const int   matTier[NUM_MATS]   = {1, 2, 3, 1, 4};
static const int   matItem[NUM_MATS]   = {B_PLANKS, B_COBBLE, I_IRON, I_GOLD, I_DIAMOND};
static const char *typeName[4] = {"Picareta", "Machado", "Pa", "Espada"};
static const char  typeChar[4] = {'P', 'M', 'A', 'E'};

Slot inv[INV_SIZE];
int fuelHalf = 0;

// ---------- Aleatorio (xorshift32) ----------
static uint32_t rs = 2463534242u;
uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
void rndSeed(uint32_t s) { rs = s ? s : 1; }

// ---------- Itens ----------
bool isTool(int id)   { return id >= I_TOOLS && id < I_TOOLS_END; }
int  toolType(int id) { return (id - I_TOOLS) % 4; }
int  toolMat(int id)  { return (id - I_TOOLS) / 4; }
int  toolMaxDur(int id) { return isTool(id) ? matDur[toolMat(id)] : 0; }
int  maxStack(int id) { return isTool(id) ? 1 : 64; }
bool isPlaceable(int id) { return id > B_AIR && id < NUM_BLOCKS && id != B_BEDROCK; }

void itemName(int id, char *buf) {
    if (id > B_AIR && id < NUM_BLOCKS) strcpy(buf, blockInfo[id].name);
    else if (id >= I_STICK && id <= I_LAST_SIMPLE) strcpy(buf, simpleItems[id - I_STICK].name);
    else if (isTool(id)) sprintf(buf, "%s de %s", typeName[toolType(id)], matName[toolMat(id)]);
    else buf[0] = 0;
}

void itemAbbr(int id, char *buf) {
    if (id > B_AIR && id < NUM_BLOCKS) { buf[0] = blockInfo[id].ab[0]; buf[1] = blockInfo[id].ab[1]; }
    else if (id >= I_STICK && id <= I_LAST_SIMPLE) { buf[0] = simpleItems[id - I_STICK].ab[0]; buf[1] = simpleItems[id - I_STICK].ab[1]; }
    else if (isTool(id)) { buf[0] = typeChar[toolType(id)]; buf[1] = matChar[toolMat(id)]; }
    else { buf[0] = '-'; buf[1] = '-'; }
    buf[2] = 0;
}

// ---------- Inventario ----------
void invClear(void) { memset(inv, 0, sizeof inv); fuelHalf = 0; }

int countItem(int id) {
    int n = 0;
    for (int i = 0; i < INV_SIZE; i++) if (inv[i].id == id) n += inv[i].count;
    return n;
}

int addItem(int id, int n) {
    int ms = maxStack(id);
    for (int i = 0; i < INV_SIZE && n > 0; i++)
        if (inv[i].id == id && inv[i].count < ms) {
            int k = ms - inv[i].count; if (k > n) k = n;
            inv[i].count += k; n -= k;
        }
    for (int i = 0; i < INV_SIZE && n > 0; i++)
        if (inv[i].id == 0) {
            int k = n > ms ? ms : n;
            inv[i].id = (uint8_t)id; inv[i].count = (uint8_t)k; inv[i].dmg = 0; n -= k;
        }
    return n;
}

bool spaceFor(int id, int n) {
    int ms = maxStack(id);
    for (int i = 0; i < INV_SIZE && n > 0; i++) {
        if (inv[i].id == 0) n -= ms;
        else if (inv[i].id == id) n -= ms - inv[i].count;
    }
    return n <= 0;
}

void removeItem(int id, int n) {
    for (int i = INV_SIZE - 1; i >= 0 && n > 0; i--)
        if (inv[i].id == id) {
            int k = inv[i].count < n ? inv[i].count : n;
            inv[i].count -= k; n -= k;
            if (!inv[i].count) inv[i].id = 0;
        }
}

// ---------- Mineracao (formula da wiki) ----------
// dano por tick = velocidade / dureza / (30 se pode coletar, 100 se nao)
// ticks = arredondar para cima(1 / dano); jogo roda a 20 ticks/s, aqui 60 fps -> x3
static int heldSpeed(int block, int heldId) {
    if (!isTool(heldId)) return 1;
    int t = toolType(heldId), want = blockInfo[block].tool;
    bool best = (t == T_PICK && want == TOOL_PICK) || (t == T_AXE && want == TOOL_AXE) ||
                (t == T_SHOVEL && want == TOOL_SHOVEL);
    return best ? matSpeed[toolMat(heldId)] : 1;
}

bool canHarvest(int block, int heldId) {
    int need = blockInfo[block].minTier;
    if (!need) return true;
    return isTool(heldId) && toolType(heldId) == T_PICK && matTier[toolMat(heldId)] >= need;
}

int breakFrames(int block, int heldId) {
    int h = blockInfo[block].hard10;
    if (h == 0xFFFF) return -1;
    if (h == 0) return 1;
    int k = canHarvest(block, heldId) ? 30 : 100;
    int num = h * k, den = heldSpeed(block, heldId) * 10;
    int ticks = (num + den - 1) / den;
    return ticks * 3;
}

void harvest(int block, int heldSlot) {
    Slot *s = &inv[heldSlot];
    int held = s->id;
    if (canHarvest(block, held)) {
        const BlockInfo *b = &blockInfo[block];
        if (block == B_LEAVES) {                         // folhas de carvalho
            uint32_t r = rnd() % 1000;
            if (r < 5) addItem(I_APPLE, 1);               // 0,5%
            else if (r < 25) addItem(I_STICK, 1 + (int)(rnd() & 1)); // 2%
        } else if (block == B_GRAVEL && rnd() % 10 == 0) {
            addItem(I_FLINT, 1);                          // 10%
        } else if (b->drop) {
            int n = b->dmin + (b->dmax > b->dmin ? (int)(rnd() % (uint32_t)(b->dmax - b->dmin + 1)) : 0);
            addItem(b->drop, n);
        }
    }
    // desgaste: 1 por bloco com dureza > 0; espadas gastam 2
    if (isTool(held) && blockInfo[block].hard10 > 0) {
        s->dmg += toolType(held) == T_SWORD ? 2 : 1;
        if (s->dmg >= toolMaxDur(held)) { s->id = 0; s->count = 0; s->dmg = 0; }
    }
}

// ---------- Crafting e fundicao ----------
Recipe recipes[64];
int numRecipes = 0;

static void addRecipe(int out, int outN, int st, int a, int an, int b, int bn) {
    Recipe *r = &recipes[numRecipes++];
    memset(r, 0, sizeof *r);
    r->out = (uint8_t)out; r->outN = (uint8_t)outN; r->station = (uint8_t)st;
    r->inId[0] = (uint8_t)a; r->inN[0] = (uint8_t)an; r->n = 1;
    if (b) { r->inId[1] = (uint8_t)b; r->inN[1] = (uint8_t)bn; r->n = 2; }
}

void initRecipes(void) {
    numRecipes = 0;
    // grade 2x2 (inventario)
    addRecipe(B_PLANKS, 4, ST_NONE, B_LOG, 1, 0, 0);
    addRecipe(I_STICK, 4, ST_NONE, B_PLANKS, 2, 0, 0);
    addRecipe(B_CTABLE, 1, ST_NONE, B_PLANKS, 4, 0, 0);
    // bancada 3x3
    addRecipe(B_FURNACE, 1, ST_TABLE, B_COBBLE, 8, 0, 0);
    for (int m = 0; m < NUM_MATS; m++) {
        addRecipe(TOOL(m, T_PICK),   1, ST_TABLE, matItem[m], 3, I_STICK, 2);
        addRecipe(TOOL(m, T_AXE),    1, ST_TABLE, matItem[m], 3, I_STICK, 2);
        addRecipe(TOOL(m, T_SHOVEL), 1, ST_TABLE, matItem[m], 1, I_STICK, 2);
        addRecipe(TOOL(m, T_SWORD),  1, ST_TABLE, matItem[m], 2, I_STICK, 1);
    }
    addRecipe(B_IRON_BLOCK, 1, ST_TABLE, I_IRON, 9, 0, 0);
    addRecipe(B_GOLD_BLOCK, 1, ST_TABLE, I_GOLD, 9, 0, 0);
    addRecipe(B_DIAMOND_BLOCK, 1, ST_TABLE, I_DIAMOND, 9, 0, 0);
    addRecipe(I_IRON, 9, ST_NONE, B_IRON_BLOCK, 1, 0, 0);
    addRecipe(I_GOLD, 9, ST_NONE, B_GOLD_BLOCK, 1, 0, 0);
    addRecipe(I_DIAMOND, 9, ST_NONE, B_DIAMOND_BLOCK, 1, 0, 0);
    // fornalha
    addRecipe(I_IRON, 1, ST_FURNACE, I_RAW_IRON, 1, 0, 0);
    addRecipe(I_GOLD, 1, ST_FURNACE, I_RAW_GOLD, 1, 0, 0);
    addRecipe(B_GLASS, 1, ST_FURNACE, B_SAND, 1, 0, 0);
    addRecipe(B_STONE, 1, ST_FURNACE, B_COBBLE, 1, 0, 0);
    addRecipe(I_CHARCOAL, 1, ST_FURNACE, B_LOG, 1, 0, 0);
}

// combustivel em meios-itens fundidos (wiki: carvao 8 itens, tronco/tabua 1,5, graveto 0,5)
static const struct { uint8_t id, half; } fuels[] = {
    {I_COAL, 16}, {I_CHARCOAL, 16}, {B_LOG, 3}, {B_PLANKS, 3}, {I_STICK, 1},
};

static bool hasFuel(int extraId, int extraN) {   // ha combustivel sem usar os ingredientes?
    if (fuelHalf >= 2) return true;
    int need = 2 - fuelHalf;
    for (unsigned i = 0; i < sizeof fuels / sizeof *fuels && need > 0; i++) {
        int c = countItem(fuels[i].id) - (fuels[i].id == extraId ? extraN : 0);
        while (c-- > 0 && need > 0) need -= fuels[i].half;
    }
    return need <= 0;
}

bool canCraft(int ri, bool hasTable, bool hasFurnace) {
    const Recipe *r = &recipes[ri];
    if (r->station == ST_TABLE && !hasTable) return false;
    if (r->station == ST_FURNACE && !hasFurnace) return false;
    for (int i = 0; i < r->n; i++) if (countItem(r->inId[i]) < r->inN[i]) return false;
    if (r->station == ST_FURNACE && !hasFuel(r->inId[0], r->inN[0])) return false;
    return true;
}

bool craft(int ri, bool hasTable, bool hasFurnace) {
    if (!canCraft(ri, hasTable, hasFurnace)) return false;
    const Recipe *r = &recipes[ri];
    for (int i = 0; i < r->n; i++) removeItem(r->inId[i], r->inN[i]);
    if (r->station == ST_FURNACE) {
        for (unsigned i = 0; i < sizeof fuels / sizeof *fuels && fuelHalf < 2; i++)
            while (fuelHalf < 2 && countItem(fuels[i].id) > 0) {
                removeItem(fuels[i].id, 1);
                fuelHalf += fuels[i].half;
            }
        fuelHalf -= 2;
    }
    if (!spaceFor(r->out, r->outN)) {     // sem espaco: devolve tudo
        for (int i = 0; i < r->n; i++) addItem(r->inId[i], r->inN[i]);
        if (r->station == ST_FURNACE) fuelHalf += 2;
        return false;
    }
    addItem(r->out, r->outN);
    return true;
}
