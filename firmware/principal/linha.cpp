/*
 * linha.cpp - posicao da linha a partir da leitura normalizada da barra
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * https://github.com/EXOTIC-TEAM-MX/EVA-RT
 * Mesmo algoritmo do perseguidor RUSH - Dreamcast; so saiu a normalizacao,
 * que agora e do barra.cpp.
 */
#include "linha.h"

#define CICLOS(ms)  ((uint16_t)(((uint32_t)(ms) * 1000UL) / PERIODO_US))

void Linha::reiniciar() {
  for (uint8_t i = 0; i < N_SENSORES; i++) _norm[i] = 0;
  for (uint8_t i = 0; i < 4; i++) _hist[i] = 0;
  _histPos = 0;
  _faixa = 0;
  _salto = 0;
  _largura = 0;
  _erro = 0;
  _vista = false;
  _cruz = false;
  _medida = false;
}

void Linha::atualizar(const uint8_t norm[N_SENSORES], bool podeCruzar) {
  const bool vistaAntes = _vista;
  _medida = false;

  uint8_t ativos = 0, serie = 0, maiorSerie = 0;
  for (uint8_t i = 0; i < N_SENSORES; i++) {
    const uint8_t n = norm[i];
    _norm[i] = n;
    if (n >= NORM_LINHA) {
      ativos++;
      if (++serie > maiorSerie) maiorSerie = serie;
    } else {
      serie = 0;
    }
  }

  _largura = maiorSerie;

  if (ativos == 0) {
    // Linha perdida. Se ela estava perto da ponta da barra, saiu por esse
    // lado: erro no extremo. Se estava no meio (falha na fita, barra
    // levantada), mantem o ultimo erro em vez de dar um tranco.
    _vista = false;
    _cruz = false;
    _faixa = 0;
    _salto = 0;
    if (_erro > LIMIAR_LATERAL) _erro = ERRO_MAX;
    else if (_erro < -LIMIAR_LATERAL) _erro = -ERRO_MAX;
    return;
  }

  _vista = true;
  // a faixa que ja comecou continua; a barra toda branca dispensa a condicao
  const bool reto = podeCruzar || _faixa > 0 || maiorSerie >= LARGURA_TOTAL;
  _cruz = reto && (maiorSerie >= LARGURA_CRUZAMENTO);

  // Faixa atravessada (linha de partida) entrando, ou ja em cima da barra,
  // num trecho reto: segue com o erro de alguns ciclos atras, de antes de ela
  // aparecer. Se ela entrar torta, por um lado da barra, puxaria a posicao.
  if (reto && maiorSerie >= LARGURA_FAIXA && _faixa < CICLOS(FAIXA_MAX_MS)) {
    if (_faixa++ == 0) _erro = _hist[_histPos];   // o mais antigo
    _salto = 0;
    return;
  }
  if (maiorSerie < LARGURA_FAIXA) _faixa = 0;

  // A media usa so o grupo de sensores ativos mais proximo de onde a linha
  // estava, mais um vizinho de cada lado (a borda da fita): um reflexo na
  // outra ponta da barra nao puxa a posicao.
  int8_t ref = (int8_t)((_erro + ERRO_MAX + 500) / 1000);
  uint8_t p = 0, menorDist = 255;
  for (uint8_t i = 0; i < N_SENSORES; i++) {
    if (_norm[i] < NORM_LINHA) continue;
    uint8_t d = (uint8_t)((int8_t)i > ref ? (int8_t)i - ref : ref - (int8_t)i);
    if (d < menorDist) {
      menorDist = d;
      p = i;
    }
  }
  uint8_t a = p, b = p;
  while (a > 0 && _norm[a - 1] >= NORM_LINHA) a--;
  while (b + 1 < N_SENSORES && _norm[b + 1] >= NORM_LINHA) b++;
  uint8_t de = a > 0 ? a - 1 : 0;
  uint8_t ate = b + 1 < N_SENSORES ? b + 1 : N_SENSORES - 1;
  if (b - a + 1 >= LARGURA_FAIXA) {
    // Faixa larga em curva fechada (barra quase paralela a linha): fica com
    // o pedaco em volta de onde a linha estava, sem saltar.
    if (p >= 3 && de < p - 3) de = p - 3;
    if (ate > p + 3) ate = p + 3;
  }
  uint16_t soma = 0, ponderada = 0;
  for (uint8_t i = de; i <= ate; i++) {
    uint8_t n = _norm[i];
    if (n > NORM_RUIDO) {
      soma += n;
      ponderada += (uint16_t)n * i;
    }
  }
  const int16_t novo = (int16_t)(((uint32_t)ponderada * 1000UL) / soma) - ERRO_MAX;

  // Num trecho reto a linha nao salta: espera confirmar (em curva a barra
  // varre a linha depressa e o salto pode ser de verdade)
  const int16_t salto = novo > _erro ? novo - _erro : _erro - novo;
  if (podeCruzar && vistaAntes && salto > SALTO_MAX && _salto < SALTO_CICLOS) {
    _salto++;
    return;
  }
  _salto = 0;
  _erro = novo;
  _medida = true;
  _hist[_histPos] = _erro;
  _histPos = (_histPos + 1) & 3;
}
