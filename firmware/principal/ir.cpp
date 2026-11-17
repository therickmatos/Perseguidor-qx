/*
 * ir.cpp - decodificadores do receptor IR (RC5 e NEC) e comandos do modulo de inicio
 *
 * Copiado sem mudancas do firmware do perseguidor RUSH - Dreamcast
 * (therickmatos/Perseguidor-de-linha), baseado no EVA-RT 1.2.2
 * (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0.
 *
 * Os dois recebem as mesmas bordas e nao se confundem: o RC5 comeca com
 * pulsos de ~0,9 ms e o NEC com um lider de 9 ms.
 */
#include "ir.h"

// ================================================================== RC5
// Decodifica pelas bordas: todo bit tem uma borda no meio; entre dois bits
// iguais ha mais uma, na fronteira. Entao, a partir da borda do meio de um
// bit, a proxima borda vem ~889 us depois (fronteira) ou ~1778 us depois
// (meio do bit seguinte). O valor do bit e o sentido da borda do meio.
// Qualquer borda fora dessas janelas, ou incoerente com o nivel, descarta
// o quadro: o RC5 nao tem soma de verificacao, a conferencia e a temporizacao.

// Janelas em us. O receptor alarga os pulsos com portadora e encurta os sem
// portadora em ate ~200 us; as janelas cobrem isso com folga.
#define FRONTEIRA_MIN   500     // nominal 889
#define FRONTEIRA_MAX  1250
#define MEIO_MIN       1300     // nominal 1778
#define MEIO_MAX       2300
#define SILENCIO_MIN   2600     // linha parada antes do primeiro bit (entre quadros: ~89 ms)
#define N_BITS           14

void DecodificadorRC5::reiniciar() {
  _estado = OCIOSO;
  _tBorda = 0;
  _tMeio = 0;
  _nivelMeio = true;
  _fronteira = false;
  _n = 0;
  _bits = 0;
  _semBorda = true;
  _q.protocolo = IR_RC5;
  _q.endereco = 0;
  _q.comando = 0;
  _q.toggle = 0;
}

bool DecodificadorRC5::borda(uint32_t us, bool nivel) {
  const bool silencio = _semBorda || (uint32_t)(us - _tBorda) >= SILENCIO_MIN;
  _tBorda = us;
  _semBorda = false;

  if (_estado == RECEBENDO) {
    const uint32_t dm = us - _tMeio;
    if (dm >= MEIO_MIN && dm <= MEIO_MAX) {
      // Meio do bit seguinte. Bits iguais: a linha voltou na fronteira e
      // agora fica no mesmo nivel do meio anterior; bits diferentes: nao
      // houve fronteira e o nivel e o oposto.
      if ((nivel == _nivelMeio) == _fronteira) {
        _bits = (uint16_t)((_bits << 1) | (nivel ? 0 : 1));   // descida = 1
        _tMeio = us;
        _nivelMeio = nivel;
        _fronteira = false;
        if (++_n < N_BITS) return false;
        // S1 (bit 13) e sempre 1 e entrou como o primeiro bit
        _q.protocolo = IR_RC5;
        _q.endereco = (uint16_t)((_bits >> 6) & 0x1F);
        _q.comando = (uint8_t)((_bits & 0x3F) | ((_bits & 0x1000) ? 0 : 0x40));
        _q.toggle = (uint8_t)((_bits >> 11) & 1);
        _estado = OCIOSO;
        return true;
      }
    } else if (dm >= FRONTEIRA_MIN && dm <= FRONTEIRA_MAX && !_fronteira && nivel != _nivelMeio) {
      _fronteira = true;
      return false;
    }
    _estado = OCIOSO;   // quadro estragado; a borda ainda pode ser o inicio de outro
  }

  // Ocioso: a primeira descida depois de a linha ficar parada e o meio do S1
  if (!nivel && silencio) {
    _estado = RECEBENDO;
    _tMeio = us;
    _nivelMeio = false;
    _fronteira = false;
    _n = 1;
    _bits = 1;
  }
  return false;
}

// ================================================================== NEC
// Mede a duracao de cada nivel na borda que o encerra. O receptor alarga os
// pulsos com portadora e encurta os sem portadora em ate ~200-300 us.
#define NEC_LIDER_MIN     7500    // nominal 9000 (com portadora)
#define NEC_LIDER_MAX    10500
#define NEC_ESPACO_MIN    3500    // nominal 4500 depois do lider
#define NEC_ESPACO_MAX    5500
#define NEC_MARCA_MIN      250    // nominal 562 (com portadora)
#define NEC_MARCA_MAX      950
#define NEC_ZERO_MIN       200    // nominal 562 (sem portadora)
#define NEC_ZERO_MAX       950
#define NEC_UM_MIN        1200    // nominal 1687
#define NEC_UM_MAX        2200

void DecodificadorNEC::reiniciar() {
  _estado = OCIOSO;
  _tBorda = 0;
  _n = 0;
  _bits = 0;
  _q.protocolo = IR_NEC;
  _q.endereco = 0;
  _q.comando = 0;
  _q.toggle = 0;
}

bool DecodificadorNEC::borda(uint32_t us, bool nivel) {
  const uint32_t dt = us - _tBorda;   // duracao do nivel que acabou (o contrario de 'nivel')
  _tBorda = us;

  if (nivel) {
    // subida: acabou um pulso com portadora de 'dt'
    if (_estado == BITS && dt >= NEC_MARCA_MIN && dt <= NEC_MARCA_MAX) {
      if (_n < 32) return false;
      // pulso final: quadro completo. O comando vem com o inverso.
      _estado = OCIOSO;
      const uint8_t a = (uint8_t)_bits, ai = (uint8_t)(_bits >> 8);
      const uint8_t c = (uint8_t)(_bits >> 16), ci = (uint8_t)(_bits >> 24);
      if ((uint8_t)(c ^ ci) != 0xFF) return false;
      _q.protocolo = IR_NEC;
      _q.comando = c;
      _q.endereco = ((uint8_t)(a ^ ai) == 0xFF) ? a : (uint16_t)(_bits & 0xFFFF);
      _q.toggle = 0;
      return true;
    }
    _estado = (dt >= NEC_LIDER_MIN && dt <= NEC_LIDER_MAX) ? LIDER : OCIOSO;
    return false;
  }

  // descida: acabou um espaco sem portadora de 'dt'
  if (_estado == LIDER) {
    if (dt >= NEC_ESPACO_MIN && dt <= NEC_ESPACO_MAX) {
      _estado = BITS;
      _n = 0;
      _bits = 0;
    } else {
      _estado = OCIOSO;   // codigo de repeticao (2,25 ms) ou ruido
    }
  } else if (_estado == BITS && _n < 32) {
    if (dt >= NEC_ZERO_MIN && dt <= NEC_ZERO_MAX) {
      _n++;
    } else if (dt >= NEC_UM_MIN && dt <= NEC_UM_MAX) {
      _bits |= (uint32_t)1 << _n;
      _n++;
    } else {
      _estado = OCIOSO;
    }
  } else {
    _estado = OCIOSO;
  }
  return false;
}

// ================================================================== comandos

ComandoIR interpretarIR(const QuadroIR &q, const RegrasIR &r) {
  if (q.protocolo == IR_NEC) {
    if (!r.nec || q.endereco != r.necEndereco) return IR_OUTRO;
    if (q.comando == r.necPartida) return IR_PARTIDA;
    if (q.comando == r.necParada) return IR_PARADA;
    return IR_OUTRO;
  }
  if (q.endereco == r.endPrograma) return IR_PROGRAMA;
  if (q.endereco != r.endPartida) return IR_OUTRO;
  const uint8_t a = (uint8_t)(r.arena & 0x7E);
  if (q.comando == (uint8_t)(a | 1)) return IR_PARTIDA;
  if (q.comando == a) return IR_PARADA;
  if (r.paradaQualquer && (q.comando & 1) == 0) return IR_PARADA;
  return IR_OUTRO;
}
