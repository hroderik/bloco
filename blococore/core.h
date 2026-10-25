// BlocoCore - regras do jogo compartilhadas entre BlocoDS e BlocoGBA
// Dados baseados na Minecraft Wiki (Java Edition): dureza, ferramenta certa,
// tier de coleta, drops, durabilidade, velocidade de ferramentas, receitas,
// fundicao e combustivel.
#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---------- Blocos (ids 0..63) ----------
enum {
    B_AIR, B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_SAND, B_GRAVEL, B_LOG, B_LEAVES,
    B_PLANKS, B_CTABLE, B_FURNACE, B_GLASS,
    B_COAL_ORE, B_IRON_ORE, B_GOLD_ORE, B_REDSTONE_ORE, B_LAPIS_ORE, B_DIAMOND_ORE, B_EMERALD_ORE,
    B_IRON_BLOCK, B_GOLD_BLOCK, B_DIAMOND_BLOCK, B_BEDROCK,
    B_SAPLING,          // muda: sem colisao, cresce em carvalho (ids antigos mantidos para o save)
    NUM_BLOCKS
};

// ---------- Itens (ids 64+) ----------
enum {
    I_STICK = 64, I_COAL, I_CHARCOAL, I_RAW_IRON, I_RAW_GOLD, I_IRON, I_GOLD,
    I_DIAMOND, I_EMERALD, I_REDSTONE, I_LAPIS, I_FLINT, I_APPLE,
    I_LAST_SIMPLE = I_APPLE,
    I_TOOLS = 80        // ferramenta = I_TOOLS + material*4 + tipo
};
enum { T_PICK, T_AXE, T_SHOVEL, T_SWORD };
enum { M_WOOD, M_STONE, M_IRON, M_GOLD, M_DIAMOND, NUM_MATS };
#define I_TOOLS_END (I_TOOLS + NUM_MATS * 4)
#define TOOL(mat, type) (I_TOOLS + (mat) * 4 + (type))

enum { TOOL_NONE, TOOL_PICK, TOOL_AXE, TOOL_SHOVEL };  // ferramenta certa do bloco
enum { DET_NONE, DET_SPOT, DET_BAND };                  // detalhe visual da face

typedef struct {
    const char *name;
    char ab[3];          // abreviacao de 2 letras para a UI
    uint8_t top[3], side[3], det[3];   // cores RGB 0..31
    uint8_t detKind;
    uint16_t hard10;     // dureza x10 (0xFFFF = inquebravel)
    uint8_t tool;        // ferramenta mais rapida
    uint8_t minTier;     // 0 = nao exige ferramenta; senao picareta de tier >= minTier
    uint8_t drop, dmin, dmax;
} BlockInfo;

extern const BlockInfo blockInfo[NUM_BLOCKS];

// ---------- Inventario ----------
typedef struct { uint8_t id, count; uint16_t dmg; } Slot;
#define INV_SIZE 36      // 0..8 = barra rapida
extern Slot inv[INV_SIZE];
extern int fuelHalf;     // combustivel da fornalha, em meios-itens fundidos

bool isTool(int id);
int  toolType(int id);
int  toolMat(int id);
int  toolMaxDur(int id);
int  maxStack(int id);
bool isPlaceable(int id);
void itemName(int id, char *buf);   // buf >= 32
void itemAbbr(int id, char *buf);   // buf >= 3

void invClear(void);
int  countItem(int id);
int  addItem(int id, int n);        // retorna o que nao coube
bool spaceFor(int id, int n);
void removeItem(int id, int n);

// ---------- Mineracao ----------
bool canHarvest(int block, int heldId);
int  breakFrames(int block, int heldId);   // em frames a 60 fps; -1 = inquebravel
void harvest(int block, int heldSlot);     // da os drops e gasta a ferramenta

// ---------- Crafting ----------
enum { ST_NONE, ST_TABLE, ST_FURNACE };
typedef struct {
    uint8_t out, outN, station, n;
    uint8_t inId[3], inN[3];
} Recipe;
extern Recipe recipes[];
extern int numRecipes;
void initRecipes(void);
bool canCraft(int r, bool hasTable, bool hasFurnace);
bool craft(int r, bool hasTable, bool hasFurnace);

// ---------- Mudas ----------
// Java: a muda recebe ~3 ticks aleatorios por 4096 blocos por tick e cada um
// tem 1/7 de chance de avancar um dos 2 estagios (~16 min de jogo). Aqui e um
// estagio so, com chance 1/14 por tick aleatorio, e mais ticks por frame
// (SAPLING_TICKS_MIN) para uma muda virar arvore em ~2 min em media.
#define SAPLING_GROW_CHANCE 14
#define SAPLING_TICKS_MIN   2          // minutos medios ate crescer
bool blocksMove(int block);            // tem colisao?
bool saplingSoil(int block);           // a muda pode ficar em cima deste bloco?
int  saplingRandomTicks(int volume);   // ticks aleatorios por frame (60 fps) para o mundo todo

// ---------- Aleatorio ----------
uint32_t rnd(void);
void rndSeed(uint32_t s);
