/*
 * hw.h - acesso ao hardware (ESP32-C3 SuperMini, placa Galileia3)
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * Mesmas funcoes do hw.h do perseguidor RUSH - Dreamcast, trocando o
 * ATmega328P pelo ESP32-C3: barra pelo modulo barra.h, TB6612 no lugar dos
 * DRV8874, EDF por ESC, sem LEDs nem medida de bateria (os avisos sao bipes
 * nos motores).
 */
#pragma once

#include <Arduino.h>
#include "config.h"
#include "barra.h"
#include "ir.h"

static_assert(N_SENSORES == NUM_SENSORS, "config.h e barra.h com numero de sensores diferente");

// Modulo da barra (calibracao, NVS, diagnostico). A leitura do ciclo e hwLerBarra().
extern Barra barra;

void hwIniciar();

// ---- barra: le os 12 sensores e devolve 0..255 (255 = linha) na ordem do
// robo (0 = esquerda), ja com a calibracao e a cor da linha do barra.cpp
void hwLerBarra(uint8_t norm[N_SENSORES]);

// ---- receptor IR (GPIO6, interrupcao nas duas bordas)
// true = chegou um quadro (RC5 ou NEC) desde a ultima chamada (fica so o mais novo)
bool hwIrQuadro(QuadroIR &q);
bool hwIrNivel();                        // nivel atual do pino (true = sem portadora)

// ---- saidas
void hwMotores(int16_t esq, int16_t dir);   // -255..255, + = para a frente, 0 = freio
void hwVentoinha(uint8_t v);                // EDF: 0 = parada (ESC armado) .. 255 = maximo
// Bipe nos motores: as rodas trocam de sentido 'hz' vezes por segundo com
// PWM 'volume' (media zero: o robo nao anda). Bloqueia por 'ms'.
void hwBipe(uint16_t ms, uint16_t hz, uint8_t volume);

// ---- botoes (true = apertado): 1 = BTN da placa (GPIO21), 2 = BOOT (GPIO9)
bool hwBotao(uint8_t n);
