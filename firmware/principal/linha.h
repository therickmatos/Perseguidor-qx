/*
 * linha.h - posicao da linha a partir da leitura normalizada da barra
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0, e no
 * firmware do perseguidor RUSH - Dreamcast (mesma escala de posicao e erro).
 *
 * Diferenca para o Dreamcast: a calibracao e a normalizacao sao do modulo
 * barra.h/.cpp (validado na bancada). Aqui entra a leitura ja normalizada,
 * 0..255 com 255 = em cima da linha, na ordem do robo (0 = esquerda).
 *
 * So contas, sem dependencia do Arduino: compila tambem no PC (firmware/testes).
 */
#pragma once

#include <stdint.h>
#include "config.h"

// Erro de -ERRO_MAX (linha sob o sensor 0, a esquerda) a +ERRO_MAX (o ultimo)
#define ERRO_MAX       ((N_SENSORES - 1) * 500)    // 3500 com 8 sensores

class Linha {
public:
  void reiniciar();                              // posicao volta para o centro

  // Uma vez por ciclo. podeCruzar = false quando o robo esta em curva: ai
  // uma faixa larga embaixo da barra e a propria linha, nao a linha de partida.
  void atualizar(const uint8_t norm[N_SENSORES], bool podeCruzar = true);

  int16_t erro() const         { return _erro; }     // -3500..+3500, + = linha a direita
  bool    vista() const        { return _vista; }
  bool    cruzamento() const   { return _cruz; }     // faixa atravessada (linha de partida) embaixo da barra
  bool    medida() const       { return _medida; }   // o erro e uma medida deste ciclo
  uint8_t norm(uint8_t i) const { return _norm[i]; } // 0..255, 255 = linha
  uint8_t largura() const      { return _largura; }  // sensores vizinhos ativos (o maior grupo)

private:
  uint8_t  _norm[N_SENSORES];
  int16_t  _erro;
  int16_t  _hist[4];     // erro dos ultimos ciclos (para antes de a faixa entrar)
  uint8_t  _histPos;
  uint8_t  _faixa;       // ciclos seguidos com o erro mantido por faixa larga
  uint8_t  _salto;       // ciclos seguidos recusando um salto
  uint8_t  _largura;
  bool     _vista;
  bool     _cruz;
  bool     _medida;
};
