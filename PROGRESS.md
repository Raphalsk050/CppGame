# Progresso

Checklist vivo do projeto. Marcar `[x]` só depois de **validado** — teste
numérico, screenshot conferido, ou medição. "Compilou" não conta como pronto.

---

## Concluído e validado

- [x] **Núcleo de marching cubes**
      Tabelas na convenção de Paul Bourke, com `edgeTable` derivada da
      `triTable` em `consteval`. Validador de invariantes cobre os 256 casos
      (toda aresta usada cruza a isosuperfície; casos complementares citam as
      mesmas arestas). *Validado: 256 casos, 820 triângulos, 2 casos vazios.*

- [x] **Gradientes na grade + margem no poligonizador**
      Normais saem por diferenças centrais sobre as amostras já existentes. Uma
      célula de padding por face evita a emenda de iluminação entre chunks.

- [x] **Chunks com streaming infinito**
      Uma única `SampleGrid` de rascunho por worker; chunk guarda só o recurso
      de GPU. Descarte barato de chunks fora da faixa de altura possível.

- [x] **Escavação esférica** (botão esq./dir.)
      Edições guardadas como lista de esferas indexada por chunk, não como
      volume. Raycast marcha o campo escalar, não a malha.

- [x] **Painel de ajuste (raygui) + save/load**
      Uma tabela de campos com ponteiro-para-membro dirige sliders E
      serialização — não há como divergirem. raygui compilado como C (o corpo
      dela é ill-formed em C++26).

- [x] **Multithreading da geração**
      Pool de workers gera geometria; thread principal só faz upload de VBO
      (contexto GL é single-thread). Token de geração descarta resultado obsoleto.
      *Validado: 6 threads, fila zerada, 59 FPS.*

- [x] **Frustum culling**
      Extração de Gribb-Hartmann dos 6 planos. *Validado por teste numérico de
      7 casos — pegou uma extração transposta que parecia funcionar.*

- [x] **Clima derivado do relevo** (não sorteado)
      Lapse rate 6,5 °C/km e efeito orográfico (sombra de chuva).
      *Validado: lapse exato (19,50 °C/100 un), barlavento mais úmido em 69%
      das serras, 0 biomas quentes acima de 90 de altitude.*

- [x] **Biomas no diagrama de Whittaker**
      Coordenadas em unidades físicas (°C, cm/ano) ancoradas em regiões reais
      (Alasca, Canadá, Noruega, Saara, Sahel, Cerrado, Amazônia). Mistura
      contínua por distância no diagrama — sem fronteira dura.
      *Validado: 9 biomas distintos numa amostra de 18×18 km.*

---

## Fila de trabalho

### 1. LOD por distância — *em andamento*
Terreno perto do jogador está grosseiro demais ("parece maçã").
- [ ] Pesquisar técnicas (transvoxel, skirts, octree de chunks)
- [ ] Decidir tratamento de **cracks** entre níveis — é o problema difícil:
      chunks vizinhos com resolução diferente não compartilham vértices
- [ ] Implementar anéis de LOD por distância
- [ ] Validar: sem rachaduras visíveis + ganho medido de FPS/triângulos

### 2. Escala e oceano
- [ ] Definir escala do jogador e garantir terreno caminhável (hoje
      `metersPerUnit = 30` no clima, mas o voxel é 1 unidade — **conferir se
      as duas escalas são coerentes**)
- [ ] Oceano de verdade: plataforma continental, profundidade crescente,
      não só um plano cortando o terreno

### 3. Superfícies fidedignas
- [ ] Rocha mais "dura": marching cubes suaviza tudo. Investigar realce de
      quinas (dual contouring / QEF) ou ruído de alta frequência só na rocha
- [ ] Diferenciar material por inclinação e altitude na geometria, não só na cor

### 4. Cavernas
- [ ] **Pesquisa extensa** antes de codar (espeleogênese, tipos de caverna,
      o que a geração do Minecraft 1.18 faz: cheese / spaghetti / noodle)
- [ ] Casar tipo de caverna com bioma e litologia
- [ ] Detalhes: câmaras, galerias, estreitamentos, entradas na superfície

### 5. Água
- [ ] Shader de água próprio (hoje é um quad chapado)
- [ ] Visão submersa: tinta azul, neblina densa, distorção
- [ ] Superfície do rio acompanhando o leito (hoje é um plano global)

### 6. Player controller
- [ ] Colisão com o terreno (contra o campo escalar, não a malha)
- [ ] Movimento estilo Minecraft: gravidade, pulo, passo, agachar
- [ ] **Modo espectador** (o voo livre atual) selecionável

### 7. Atmosfera
- [ ] **Pesquisar** espalhamento Rayleigh/Mie antes de implementar
- [ ] Céu com gradiente físico, sol, névoa aérea por distância

### 8. Estruturas pré-definidas — *por último*
- [ ] Base para instanciar modelos (árvores, casas)
- [ ] Consultar a lista de estruturas do Minecraft
- [ ] Posicionamento por bioma, altitude e inclinação

---

## Regras aprendidas (não repetir os erros)

1. **Tudo que soma ou multiplica ALTURA tem de ser contínuo em (x,z).**
   Modular montanha por bioma discreto criou paredões verticais nas fronteiras.

2. **Rio puxando a altura até um leito absoluto corta canyon proporcional à
   altura do terreno.** Precisa de comporta por altitude *e* por concavidade.

3. **Bioma escolhe cor, nunca altura.** E a cor tem de ser mistura contínua:
   escolha discreta produz linha reta visível no campo.

4. **Verificar com print, não com "compilou".** A extração do frustum estava
   transposta e parecia funcionar; só o teste numérico pegou.

5. **A raylib guarda `M[linha][coluna]` em `m[coluna*4+linha]`.**

6. **Neblina exponencial simples lava a cena inteira.** Usar exp do quadrado.
