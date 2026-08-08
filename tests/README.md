# Validação

Testes que rodam o gerador **sem janela** e conferem propriedades numéricas.
Existem porque erro de terreno é fácil de olhar e achar que está certo — a
extração do frustum estava transposta e "parecia" funcionar, e o ruído 3D de
saliência estava sete vezes abaixo do limiar sem que a tela denunciasse.

## Rodar

```sh
./tests/run.sh
```

## O que cada um cobre

**`climate_validation.cpp`**
- Lapse rate confere com 6,5 °C/km e o exagero vertical declarado
- Sombra de chuva: barlavento mais úmido que sotavento na maioria das serras
- Variedade de biomas numa área grande
- Coerência: nenhum bioma quente acima da linha de neve

**`terrain_validation.cpp`**
- Escala: relevo em metros, coerente com um jogador de 1,8 m
- Caminhabilidade: fração do terreno abaixo de 45° para um passo de 1 m
- Detalhe fino: rugosidade medida em escala de 3 m (pega "terreno liso demais")
- **Saliências 3D**: conta cruzamentos da densidade numa coluna vertical.
  Heightfield tem exatamente 1 por definição; mais de 1 prova que existe
  geometria que um mapa de altura não representa
