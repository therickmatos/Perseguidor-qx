/*
 * ir.h - decodificadores do receptor IR (RC5 e NEC) e comandos do modulo de inicio
 *
 * Copiado sem mudancas do firmware do perseguidor RUSH - Dreamcast
 * (therickmatos/Perseguidor-de-linha), baseado no EVA-RT 1.2.2
 * (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0.
 *
 * So contas: recebe o instante (us) e o nivel de cada borda da saida do
 * receptor (KY-022: nivel baixo = portadora presente). Quem le o pino e
 * chama borda() e a interrupcao do hw.cpp. Sem dependencia do Arduino, para
 * ser testado no PC (firmware/testes/teste_ir.cpp).
 *
 * RC5 (Philips): e o do modulo de inicio do juiz. 14 bits Manchester de
 * 1778 us, o mais significativo primeiro: S1 S2 T A4..A0 C5..C0. "1" = meio
 * bit sem portadora e meio com (na saida do receptor: alto e depois baixo);
 * "0" = o contrario. S1 = 1; S2 = 1 no RC5 e o bit 6 do comando, invertido,
 * no RC5 estendido. T muda a cada aperto do botao do controle.
 *
 * NEC: e o do controle que vem com o KY-022. Lider de 9 ms com portadora e
 * 4,5 ms sem; 32 bits, o menos significativo primeiro (endereco, endereco
 * invertido, comando, comando invertido), cada um com 562 us de portadora e
 * 562 us (0) ou 1687 us (1) sem. Botao segurado: codigo de repeticao (lider
 * de 9 ms + 2,25 ms), que aqui e ignorado. E o controle usado no TRIF (ver config.h).
 */
#pragma once

#include <stdint.h>

#define IR_RC5  0
#define IR_NEC  1

struct QuadroIR {
  uint8_t  protocolo;   // IR_RC5 ou IR_NEC
  uint8_t  comando;     // RC5: 0..127 (bit 6 = S2 invertido); NEC: 0..255
  uint16_t endereco;    // RC5: 0..31; NEC: 0..255 (16 bits no NEC estendido)
  uint8_t  toggle;      // RC5: 0 ou 1; NEC: sempre 0
};

class DecodificadorRC5 {
public:
  void reiniciar();

  // Uma borda da saida do receptor: instante em us (pode dar a volta em
  // 2^32) e nivel DEPOIS da borda (false = baixo = portadora).
  // Devolve true quando um quadro acaba de ser completado (ver quadro()).
  bool borda(uint32_t us, bool nivel);

  const QuadroIR &quadro() const { return _q; }

private:
  enum Estado : uint8_t { OCIOSO, RECEBENDO };
  Estado    _estado;
  uint32_t  _tBorda;     // ultima borda (qualquer)
  uint32_t  _tMeio;      // ultima borda de meio de bit
  bool      _nivelMeio;  // nivel depois da ultima borda de meio de bit
  bool      _fronteira;  // ja houve a borda de fronteira desde o ultimo meio
  bool      _semBorda;   // nenhuma borda desde reiniciar()
  uint8_t   _n;          // bits recebidos
  uint16_t  _bits;
  QuadroIR  _q;
};

class DecodificadorNEC {
public:
  void reiniciar();
  bool borda(uint32_t us, bool nivel);   // como no RC5
  const QuadroIR &quadro() const { return _q; }

private:
  enum Estado : uint8_t { OCIOSO, LIDER, BITS };
  Estado   _estado;
  uint32_t _tBorda;
  uint8_t  _n;
  uint32_t _bits;
  QuadroIR _q;
};

// ------------------------------------------------------------------ modulo de inicio
// Significado de um quadro. RC5: padrao do startmodule.com (ver config.h).
// NEC: so se estiver ligado, com as teclas escolhidas no console.
enum ComandoIR : uint8_t {
  IR_NADA,       // nenhum quadro novo (nao sai de interpretarIR)
  IR_OUTRO,      // quadro valido, mas nao e para este robo
  IR_PARTIDA,
  IR_PARADA,
  IR_PROGRAMA    // o juiz mandou gravar um numero de arena novo (so RC5)
};

struct RegrasIR {
  uint8_t  arena;           // numero gravado (par)
  bool     paradaQualquer;  // aceita a parada de qualquer arena
  uint8_t  endPrograma;     // RC5
  uint8_t  endPartida;      // RC5
  bool     nec;             // aceita o controle NEC (teste)
  uint16_t necEndereco;
  uint8_t  necPartida;
  uint8_t  necParada;
};

ComandoIR interpretarIR(const QuadroIR &q, const RegrasIR &r);

// Numero de arena gravado por um quadro de programacao
inline uint8_t arenaDoQuadro(const QuadroIR &q) {
  return (uint8_t)(q.comando & 0x7E);
}
