// BlocoCore - graficos 2D gerados por codigo (tiles 8x8 de 16 cores)
// Usado pelo GBA (tela toda) e pela tela de baixo do DS (mesmo hardware 2D).
#pragma once
#include <stdint.h>

#define ART_RGB(r, g, b) ((uint16_t)((r) | ((g) << 5) | ((b) << 10)))

// Banco 0: terreno e jogador. Banco 1: minerios, metais e itens.
extern const uint16_t artPal[2][16];

#define ART_WALL   30     // parede de caverna (terra); depois dos ids de bloco
#define ART_WALL2  31     // parede de caverna (pedra)
#define ART_PLAYER 120    // 8x16 (2 tiles)
#define ART_FRAME  122    // moldura de selecao
#define ART_DOT    123    // ponto do jogador no minimapa
#define ART_VIEW   124    // ponto da direcao do olhar

int iconBank(int id);                 // banco de paleta do bloco/item
int iconPixel(int id, int x, int y);  // cor 0..15 (0 = transparente) de blocos, paredes e itens
int extraPixel(int id, int x, int y); // ART_PLAYER (y 0..15), ART_FRAME, ART_DOT, ART_VIEW

// Escreve um tile 8x8 4bpp (8 palavras de 32 bits)
void artTile(volatile uint32_t *dst, int id);
// Escreve um icone 16x16 ampliado 2x (4 tiles em ordem 1D: sup-esq, sup-dir, inf-esq, inf-dir)
void artTile16(volatile uint32_t *dst, int id);
// Escreve um glifo da fonte 5x7: cor do texto, cor da sombra (0 = sem), cor do fundo
void artGlyph(volatile uint32_t *dst, int ch, int fg, int shadow, int bg);
