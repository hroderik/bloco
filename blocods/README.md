# BlocoDS

Clone de blocos estilo Minecraft para Nintendo DS, escrito do zero em C com libnds.
Não usa código nem assets do Minecraft: todos os gráficos são gerados por código.

## Recursos (fase 2: sobrevivência básica)
- Mundo 48×32×48 com morros, árvores, cavernas, cascalho e 7 minérios por profundidade
- Mineração com tempo real (fórmula da Minecraft Wiki), tier de picareta e drops corretos
- 20 ferramentas (picareta, machado, pá e espada × madeira, pedra, ferro, ouro, diamante)
- Inventário de 36 slots, crafting 2×2, bancada de trabalho e fornalha com combustível
- **Tela de baixo gráfica por toque**, com 4 abas:
  - **MUNDO**: minimapa (toque para alternar superfície/caverna), item na mão, mira, barra de quebra, estações por perto, barra rápida
  - **MOCHILA**: 36 slots com ícones; toque para pegar e toque no destino para trocar
  - **CRIAR**: lista com ícones, filtro "possíveis", detalhes com ingredientes e botão CRIAR
  - **MENU**: salvar, carregar, novo mundo e opções (sensibilidade, inverter Y, distância de visão)
- Save em arquivo `blocods.sav` no cartão (FAT)

## Controles
| Botão | Ação |
|---|---|
| D-pad | andar |
| L / R | girar |
| Stylus arrastando (aba MUNDO) | olhar |
| B | pular |
| Y (segurar) | quebrar |
| A | colocar bloco |
| X | próximo slot da barra |
| SELECT | trocar de aba |
| START | abrir o menu |

## Como compilar
1. Instale o BlocksDS: https://blocksds.skylyrac.net/docs/setup/
   (ou com Docker: `docker pull skylyrac/blocksds:slim-latest`)
2. Na pasta `blocods`: `make` (a pasta `blococore` precisa estar ao lado)
3. Abra `BlocoDS.nds` no melonDS, DeSmuME ou num flashcard. O save precisa de cartão/DLDI.

Com Docker, a partir da pasta que contém `blocods` e `blococore`:
```
docker run --rm -v "$PWD":/work -w /work/blocods skylyrac/blocksds:slim-latest make
```

Com devkitPro: use o template `arm9`, copie `source/` e os arquivos de `../blococore`
para a pasta de fontes e adicione `-lfat` antes de `-lnds9` em `LIBS`.

## Prints
`make preview` compila o mesmo código do jogo no PC, com um rasterizador 3D por software
para a tela de cima e um emulador do motor 2D para a tela de baixo, joga cenários
roteirizados e gera as imagens em `preview/`.

## Otimizações
- Ponto fixo 20.12 (o DS não tem FPU)
- Máscara de faces expostas pré-calculada, atualizada só ao editar
- Só desenha faces viradas para a câmera, dentro do raio de visão
- Limite de 1450 quads por frame (o DS aguenta ~1536)
- Interface redesenhada só quando algo muda; minimapa só quando o mundo muda
