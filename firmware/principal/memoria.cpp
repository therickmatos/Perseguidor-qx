/*
 * memoria.cpp - parametros e numero da arena (IR) na NVS do ESP32-C3
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 */
#include <Preferences.h>
#include <string.h>
#include "memoria.h"

static const char *const NVS_ROBO = "robo";
static const char *const CHAVE_PAR = "par";
static const char *const CHAVE_ARENA = "arena";

// Mudar a struct Parametros -> somar 1 aqui (a gravada antiga e ignorada)
#define MAGIA_PARAMETROS 0x5101

struct BlocoParametros {
  uint16_t   magia;
  uint16_t   tamanho;
  Parametros p;
};

bool memCarregarParametros(Parametros &p) {
  Preferences nvs;
  // leitura/escrita: no primeiro boot cria o namespace em vez de dar erro
  if (!nvs.begin(NVS_ROBO, false)) return false;
  BlocoParametros b;
  bool ok = nvs.getBytesLength(CHAVE_PAR) == sizeof(b) &&
            nvs.getBytes(CHAVE_PAR, &b, sizeof(b)) == sizeof(b) &&
            b.magia == MAGIA_PARAMETROS && b.tamanho == sizeof(Parametros);
  nvs.end();
  if (ok) p = b.p;
  return ok;
}

bool memSalvarParametros(const Parametros &p) {
  BlocoParametros b;
  memset(&b, 0, sizeof(b));
  b.magia = MAGIA_PARAMETROS;
  b.tamanho = sizeof(Parametros);
  b.p = p;
  Preferences nvs;
  if (!nvs.begin(NVS_ROBO, false)) return false;
  bool ok = nvs.putBytes(CHAVE_PAR, &b, sizeof(b)) == sizeof(b);
  nvs.end();
  return ok;
}

bool memCarregarArena(uint8_t &arena) {
  Preferences nvs;
  if (!nvs.begin(NVS_ROBO, false)) return false;
  bool ok = nvs.isKey(CHAVE_ARENA);
  uint8_t a = ok ? nvs.getUChar(CHAVE_ARENA, 0xFF) : 0xFF;
  nvs.end();
  if (!ok || a > 126 || (a & 1)) return false;
  arena = a;
  return true;
}

bool memSalvarArena(uint8_t arena) {
  Preferences nvs;
  if (!nvs.begin(NVS_ROBO, false)) return false;
  bool ok = nvs.putUChar(CHAVE_ARENA, arena) == 1;
  nvs.end();
  return ok;
}
