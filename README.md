# Bloco - clones de blocos para DS e GBA

- `blococore/` - regras compartilhadas: blocos, itens, receitas, mineração, inventário
  (dados da Minecraft Wiki) e gráficos 2D gerados por código, além da fonte 5×7
- `blocods/` - versão 3D para Nintendo DS, com interface por toque na tela de baixo
- `blocogba/` - versão 2D (vista lateral) para Game Boy Advance

Mantenha as três pastas lado a lado. Instruções de compilação no README de cada projeto.

## Dados de jogo usados (Java Edition)
- Tempo de quebra: dano/tick = velocidade ÷ dureza ÷ (30 se pode coletar, senão 100)
- Velocidade: mão 1, madeira 2, pedra 4, ferro 6, diamante 8, ouro 12
- Durabilidade: madeira 59, pedra 131, ferro 250, ouro 32, diamante 1561
- Tier de coleta: pedra/carvão = madeira; ferro/lápis = pedra; ouro/redstone/diamante/esmeralda = ferro
- Combustível: carvão 8 itens, tronco/tábua 1,5, graveto 0,5
- Drops: folhas (graveto 2%, maçã 0,5%), cascalho (pederneira 10%), redstone 4-5, lápis 4-9

## Simplificações
- Mundo pequeno e altura comprimida; minérios distribuídos por profundidade relativa
- Fornalha funde na hora, gastando combustível de um estoque global
- Sem mudas, mobs, vida, fome, Nether e End ainda (próximas fases)
