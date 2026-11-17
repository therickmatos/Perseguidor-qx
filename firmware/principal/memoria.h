/*
 * memoria.h - parametros e numero da arena (IR) na NVS do ESP32-C3
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * Mesmas funcoes do memoria.h do Dreamcast (que usa a EEPROM do ATmega).
 * A calibracao da barra e a cor da linha ficam com o barra.cpp, no
 * namespace "barra" (o mesmo do teste_barra: uma calibracao feita la vale aqui).
 */
#pragma once

#include <stdint.h>
#include "params.h"

bool memCarregarParametros(Parametros &p);   // false = nada valido gravado
bool memSalvarParametros(const Parametros &p);

// Numero da arena do modulo de inicio (gravado pelo comando de programar do
// juiz, como no modulo de verdade). false = nunca foi gravado.
bool memCarregarArena(uint8_t &arena);
bool memSalvarArena(uint8_t arena);
