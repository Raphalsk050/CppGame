# Progresso

Checklist vivo. Marcar `[x]` só depois de **validado** — teste numérico,
screenshot conferido ou medição. "Compilou" não conta como pronto.

---

## Concluído e validado

- [x] **Núcleo de marching cubes** — tabelas de Paul Bourke, `edgeTable`
      derivada da `triTable` em `consteval`, validador cobrindo os 256 casos.
      *Validado: 256 casos, 820 triângulos, 2 vazios.*
- [x] **Gradientes na grade + margem** — normais por diferenças centrais; 1
      célula de padding por face elimina emenda de iluminação entre chunks.
- [x] **Chunks com streaming infinito** — grade de rascunho por worker; chunk
      guarda só o recurso de GPU.
- [x] **Escavação esférica** — edições como lista de esferas por chunk; raycast
      marcha o campo escalar, não a malha.
- [x] **Painel raygui + save/load** — uma tabela com ponteiro-para-membro
      dirige sliders *e* serialização.
- [x] **Multithreading** — workers geram, thread principal só sobe VBO.
      *Validado: 6 threads, fila zerada, 59 FPS.*
- [x] **Frustum culling** — Gribb-Hartmann. *Validado por teste de 7 casos, que
      pegou uma extração transposta que "parecia" funcionar.*
- [x] **Clima derivado do relevo** — lapse rate 6,5 °C/km + efeito orográfico.
      *Validado: lapse exato, barlavento mais úmido em 69% de 1515 serras.*
- [x] **Biomas de Whittaker** em unidades físicas ancoradas em regiões reais.
      *Validado: 9 biomas distintos em 18×18 km, 0 bioma quente acima de 90.*

---

# ORDEM DE EXECUÇÃO E DEPENDÊNCIAS

A ordem **não** é por gosto: várias tarefas quebram se feitas antes das que
elas dependem.

```
        [0] ESCALA DO MUNDO          <-- BLOQUEIA QUASE TUDO
             │   define metro/unidade; sem isso jogador, clima,
             │   nuvem e caverna usam réguas incompatíveis
             ▼
    ┌────────┼──────────────┬──────────────┐
    ▼        ▼              ▼              ▼
[1] DETALHE  [6] PLAYER   [3] OCEANO    [4] CAVERNAS
 superfície   controller   batimetria    (precisam da escala
    │            │             │          p/ o jogador caber)
    ▼            │             ▼
[2] LOD          │         [5] ÁGUA
 (só faz         │          shader+submerso
  sentido com    │             │
  detalhe a      │             ▼
  preservar)     │         [5b] BIOMAS AQUÁTICOS
    └────────┬───┘
             ▼
        [7] ATMOSFERA  ──▶  [8] NUVENS
         (céu por scattering)  (usam o clima já pronto)
             │
             ▼
        [9] ESTRUTURAS
         (dependem de bioma, inclinação e escala)
```

**Prioridade:**

| # | tarefa | prioridade | bloqueada por |
|---|---|---|---|
| 0 | Escala do mundo | **crítica** | — |
| 1 | Detalhe de superfície | **alta** (é o que o olho reclama) | 0 |
| 2 | LOD | alta (vira crítica após 0) | 0, 1 |
| 3 | Oceano / batimetria | média | 0 |
| 4 | Cavernas | média | 0 |
| 5 | Água + submerso | média | 3 |
| 6 | Player controller | média | 0 |
| 7 | Atmosfera | baixa | — |
| 8 | Nuvens | baixa | 7 |
| 9 | Estruturas | baixa (por último) | 0, 1, 4 |

---

# [0] ESCALA DO MUNDO — *bloqueante, fazer primeiro*

**Incoerência encontrada na revisão:** o clima usa `metersPerUnit = 30`, mas o
voxel tem 1 unidade. Se 1 unidade fossem 30 m, um jogador de 1,8 m mediria
0,06 voxel — impossível de representar. As duas escalas foram escolhidas em
momentos diferentes e nunca casaram.

- [ ] Fixar **1 unidade = 1 metro**, voxel de 1 m (como no Minecraft: bloco de
      1 m, jogador de 1,8)
- [ ] Reescalar o clima: `metersPerUnit = 1`. Para o lapse rate continuar
      visível, as montanhas precisam de **altura em metros reais**
      (500–2000 m), não 100
- [ ] Consequência: o mundo fica ~20× maior em unidades → **torna o LOD
      obrigatório** e exige revisar todas as escalas de ruído
- [ ] Validar: altura de montanha, temperatura no cume, passo do jogador e
      largura de caverna — tudo na mesma régua

---

# FILA DE TRABALHO

## 1. Detalhe de superfície — *causa real do "parece macinha de modelar"*

Diagnóstico: **não é falta de LOD.** O terreno é liso porque a função de altura
não tem conteúdo de alta frequência. `hillScale = 0.011` dá comprimento de onda
de ~90 unidades; abaixo disso não existe nada. Aumentar resolução do voxel não
inventa detalhe que a função não tem.

- [ ] Camada de detalhe fino: fbm de alta frequência (escala 0.05–0.15,
      amplitude 1–3), modulada por inclinação — rocha recebe mais, planície
      menos
- [ ] Ruído 3D de baixa amplitude direto na densidade (não na altura): é o que
      cria saliências e reentrâncias que um heightfield não consegue
- [ ] **Rocha "dura"**: marching cubes arredonda tudo. Investigar
      - realce de quina por ruído ridged de alta frequência só onde é íngreme
      - ou dual contouring com QEF (preserva aresta, mas troca o mesher inteiro)
- [ ] Validar: medir variação de normal entre vértices vizinhos antes/depois

## 2. LOD por distância

**Pesquisa feita.** O problema difícil é a **rachadura entre níveis**: chunks
vizinhos com resolução diferente não compartilham vértices na fronteira.

Opções levantadas:

| técnica | como funciona | custo |
|---|---|---|
| **Transvoxel** (Lengyel, 2009) | célula de transição com 13 pontos em vez de 8, 512 configurações tabeladas | correto e sem artefato, mas exige transcrever uma tabela grande — mesmo risco da triTable, precisa de validador próprio |
| **Skirts / saias** | cortina vertical na borda do chunk escondendo a fenda | trivial e robusto; artefato é uma parede fina, quase invisível |
| **Dual contouring com seams** | gera malha de costura separada entre níveis | exige trocar o mesher |

- [ ] **Decisão:** começar por *skirts* (robusto, sem tabela nova), medir, e só
      partir para Transvoxel se a saia aparecer
- [ ] Anéis de LOD por distância (voxel 1 perto, 2, 4 longe)
- [ ] **Validar numericamente**: gerar dois chunks vizinhos em níveis
      diferentes e conferir se há vértice de fronteira sem par — dá para
      detectar rachadura sem depender do olho
- [ ] Medir ganho: triângulos e FPS antes/depois

Fontes: [Transvoxel](https://transvoxel.org/),
[Nick's Voxel Blog — seams & LOD](http://ngildea.blogspot.com/2014/09/dual-contouring-chunked-terrain.html)

## 3. Escala e oceano

- [ ] **Conferir coerência de escala** — hoje `metersPerUnit = 30` no clima mas
      o voxel tem 1 unidade. Se 1 unidade = 30 m, um jogador de 1,8 m tem 0,06
      voxel: incoerente. Decidir a escala real e propagar
- [ ] Terreno caminhável na escala do jogador (inclinação máxima, largura de
      passagem)
- [ ] Oceano: plataforma continental, talude, profundidade crescente — hoje é
      um plano cortando o terreno

## 4. Cavernas — *pesquisa feita*

Espeleogênese real, para a geração não ser chute:

- **Zona freática** (abaixo do lençol): dissolução em todas as direções →
  condutos **tubulares**, seção arredondada
- **Zona vadosa** (acima do lençol): água percola por gravidade → **poços
  verticais** e canyons entrincheirados de seção alta e estreita
- **Epigênico** (água da superfície, recarga por chuva) → padrão **dendrítico**,
  galerias que se juntam como afluentes
- **Hipogênico** (fluidos ascendentes, ácido profundo) → padrão **labiríntico**,
  malha de galerias interconectadas

Consequências para a implementação:
- [ ] Definir um **nível de lençol freático** e mudar o perfil da caverna
      conforme a profundidade relativa a ele: tubular abaixo, fenda vertical acima
- [ ] Cavernas do Minecraft 1.18 como referência de forma:
      **cheese** (região do ruído > limiar → câmaras grandes),
      **spaghetti** e **noodle** (borda entre positivo e negativo do ruído →
      túnel longo conectado). Hoje só tenho a variante spaghetti
- [ ] **Casar com bioma e litologia**: caverna cárstica precisa de rocha
      solúvel — mais frequente onde há calcário; deserto tem menos dissolução
- [ ] Entradas na superfície (hoje as cavernas não afloram de propósito)
- [ ] Validar: medir volume vazio por faixa de profundidade e conectividade

Fontes: [Speleogenesis: Evolution of Karst Aquifers](https://www.researchgate.net/publication/220044229_Speleogenesis_Evolution_of_Karst_Aquifers),
[Karst topography and cave formation](https://geologyscience.com/geology-branches/sedimentology/karst-topography-and-cave-formation/),
[Minecraft Wiki — Cave](https://minecraft.wiki/w/Cave)

## 5. Água — *pesquisa feita*

Classificação de **Jerlov** para tipos de água (I, IA, IB, II, III oceânicas;
1–9 costeiras), definida pelo coeficiente de atenuação difusa **Kd**:

- Água tipo I (mais limpa): 10% da luz ainda chega a **90 m**
- Absorção é **seletiva por comprimento de onda**: o vermelho some primeiro,
  por isso o oceano limpo é azul-profundo
- Água costeira tem mais partículas em suspensão: espalha mais, e o **verde
  penetra mais fundo** que o azul → tom esverdeado

- [ ] Cor da água por **profundidade e tipo**, com absorção por canal (vermelho
      atenua muito mais rápido que azul)
- [ ] Shader de água próprio: Fresnel, reflexo do céu, ondas
- [ ] **Visão submersa**: névoa densa com a cor do tipo de água, perda de
      contraste com a distância, tinta na tela
- [ ] Biomas aquáticos: oceano frio/quente/congelado, recife, abismo — a cor
      muda com temperatura e profundidade
- [ ] Superfície de rio acompanhando o leito (hoje é um plano global no nível
      do mar)

Fontes: [Jerlov water types — IOPs](https://www.researchgate.net/publication/364766106_Measured_IOPs_of_Jerlov_water_types),
[Light in the Ocean (LibreTexts)](https://geo.libretexts.org/Bookshelves/Oceanography/Introduction_to_Physical_Oceanography_(Stewart)/06:_Temperature_Salinity_and_Density/6.10:_Light_in_the_Ocean_and_Absorption_of_Light)

## 6. Player controller

- [ ] Colisão contra o **campo escalar**, não contra a malha (o campo é
      consultável em qualquer ponto; a malha está na GPU e fatiada)
- [ ] Cápsula do jogador, gravidade, pulo, degrau, agachar
- [ ] **Modo espectador** (o voo livre atual) selecionável por tecla
- [ ] Validar: não atravessar parede em velocidade alta (varredura, não teste
      pontual)

## 7. Atmosfera — *pesquisa feita*

Coeficientes reais para espalhamento:

- **Rayleigh** (moléculas de ar): β = (5,8; 13,5; 33,1)×10⁻⁶ m⁻¹ para
  λ = (680, 550, 440) nm. A razão entre os canais é o que dá o azul do céu e o
  vermelho do poente — não precisa ser inventada
- **Mie** (aerossóis): β ≈ 2,2×10⁻⁵ m⁻¹, praticamente sem dependência de
  comprimento de onda; densidade cai exponencialmente com altura de escala
  **H ≈ 1,2 km**
- Rayleigh tem altura de escala ≈ 8 km

- [ ] Céu por espalhamento, não gradiente pintado à mão
- [ ] Sol com posição controlável; poente avermelhado sai de graça da física
- [ ] Névoa aérea consistente com o céu (hoje é uma cor fixa casada na mão)

Fontes: [Bruneton & Neyret — Precomputed Atmospheric Scattering](https://inria.hal.science/inria-00288758/document),
[Scratchapixel — Simulating the Colors of the Sky](https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html)

## 7b. Nuvens e clima — *pesquisa feita*

**A altitude define o tipo** (classificação OMM / NOAA):

| nível | altitude | tipos |
|---|---|---|
| alto | 5–13 km | cirrus, cirrocumulus, cirrostratus — **cristal de gelo**, fibrosas |
| médio | 2–7 km | altocumulus, altostratus, nimbostratus |
| baixo | 0–2 km | stratus, cumulus, stratocumulus, cumulonimbus |

**A base da nuvem tem fórmula fechada** — o *lifted condensation level*:

> **LCL ≈ 125 × (T − Td) metros**

com T = temperatura de superfície e Td = ponto de orvalho. Isso amarra a nuvem
ao clima que **já está implementado**: onde o ar é úmido (Td perto de T) a
nuvem é baixa; onde é seco, alta ou ausente. Não precisa de ruído novo.

Formas, pela física:
- **Cumulus** — convecção: ar aquecido sobe em coluna. Base plana no LCL, topo
  encaracolado. Nascem sobre terreno aquecido
- **Stratus** — camada estável, cobertura uniforme, sem convecção
- **Cirrus** — só gelo, no alto, fibrosas
- **Orográficas** — formam-se a barlavento da serra, pelo **mesmo mecanismo
  que já uso para a sombra de chuva**: nuvem e precipitação casam de graça

- [ ] Base da nuvem pelo LCL, a partir da temperatura e umidade existentes
- [ ] Tipo por altitude e estabilidade
- [ ] Cobertura correlacionada à precipitação do bioma — deserto com céu limpo,
      floresta tropical encoberta
- [ ] Nuvem orográfica presa à serra, a barlavento
- [ ] Validar: fração de céu encoberto por bioma contra o esperado

**Köppen como teste de validação independente** (A tropical, B árido,
C temperado, D continental, E polar):
- Tropical A: todo mês acima de **18 °C**
- Polar E: mês mais quente abaixo de **10 °C** (limite do crescimento arbóreo)
- Árido B: limiar em mm = 20 × (temperatura média anual em °C) + (280 se ≥70%
  da chuva no verão; 140 se 30–70%; 0 se <30%). Abaixo de 50% do limiar =
  deserto (BW); entre 50 e 100% = estepe (BS)

- [ ] Classificar o mundo gerado por Köppen e comparar a distribuição com a da
      Terra — é um teste que não depende de opinião sobre "ficou bonito"

Fontes: [NOAA — Four Core Types of Clouds](https://www.noaa.gov/jetstream/clouds/four-core-types-of-clouds) ·
[NWS — Cloud Classification](https://www.weather.gov/lmk/cloud_classification) ·
[Köppen climate classification](https://en.wikipedia.org/wiki/K%C3%B6ppen_climate_classification)

## 8. Estruturas pré-definidas — *por último*

- [ ] Base para instanciar modelos (árvore, casa) com transform por instância
- [ ] Consultar a lista de estruturas do Minecraft (vila, poço, ruína, templo)
- [ ] Posicionamento por bioma, altitude e **inclinação** (casa não nasce em
      encosta de 40°)
- [ ] Densidade de árvore por precipitação — o dado já existe no clima

---

## Regras aprendidas (não repetir)

1. **Tudo que soma ou multiplica ALTURA tem de ser contínuo em (x,z).**
   Modular montanha por bioma discreto criou paredões verticais.
2. **Rio puxando altura até leito absoluto corta canyon proporcional à altura
   do terreno.** Precisa de comporta por altitude *e* concavidade.
3. **Bioma escolhe cor, nunca altura** — e a cor tem de ser mistura contínua.
4. **Validar com número, não com "compilou".** O frustum estava transposto e
   parecia funcionar.
5. **raylib guarda `M[linha][coluna]` em `m[coluna*4+linha]`.**
6. **Neblina exponencial simples lava a cena.** Usar exp do quadrado.
7. **Aumentar resolução não cria detalhe** que a função de altura não tem.
8. **Escalas precisam casar entre subsistemas.** Clima medido em metros e voxel
   em unidades arbitrárias produziram um mundo onde o jogador não cabe.
