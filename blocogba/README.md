# BlocoGBA

Clone 2D de blocos (estilo Minecraft visto de lado) para Game Boy Advance.
Escrito do zero em C, sem bibliotecas, falando direto com o hardware.
Não usa código nem assets do Minecraft: todos os gráficos são gerados por código.

## Recursos (fase 2: sobrevivência básica)
- Mundo de 256×64 com morros, árvores, cavernas, cascalho e 7 minérios por profundidade
  (carvão no alto, diamante e redstone perto da rocha-matriz)
- Mineração com tempo real (fórmula da Minecraft Wiki), tier de picareta e drops corretos
- 20 ferramentas, inventário de 36 slots, crafting 2×2, bancada e fornalha com combustível
- HUD com barra rápida de ícones, nome do item, durabilidade e barra de quebra
- Menu com abas INVENTÁRIO e CRIAR, fonte própria 5×7 e ícones de todos os itens
- **Save no cartucho (SRAM)**: mundo, inventário e combustível

## Controles
| Botão | Ação |
|---|---|
| ← → | andar |
| A ou ↑ | pular |
| B + D-pad | mover a mira (até 4 blocos) |
| R (segurar) | quebrar |
| L | colocar bloco |
| SELECT | próximo slot da barra |
| START | abrir/fechar menu |
| No menu: L/R | trocar de aba |
| No menu: A | pegar/trocar slot ou criar |
| No menu: SELECT | salvar |
| No menu: L+R+SELECT | novo mundo |

## Como compilar
1. Instale o devkitPro com devkitARM: https://devkitpro.org/wiki/Getting_Started
2. Na pasta `blocogba`: `make` (a pasta `blococore` precisa estar ao lado)
3. Abra `BlocoGBA.gba` no mGBA, VisualBoyAdvance ou num flashcard.
   Configure o save como SRAM se o emulador perguntar.

## Prints
`make preview` compila o mesmo código do jogo no PC, joga cenários roteirizados
e desenha a tela do GBA (modo 0, 3 BGs, sprites e paletas) em imagens `.ppm`.

## Como funciona
- Modo 0: BG0 = blocos, BG1 = paredes de caverna, BG2 = texto/HUD, sprites = jogador, mira e ícones
- Dois bancos de paleta (terreno e minérios/itens) para caber tudo em 16 cores por tile
- Mapas de 32×32 tiles com rolagem circular: só a área visível é redesenhada,
  e só quando a câmera cruza um tile ou um bloco muda
- Física em ponto fixo 24.8, movimento pixel a pixel
- Mundo em EWRAM (16 KB), salvo inteiro na SRAM de 32 KB do cartucho
