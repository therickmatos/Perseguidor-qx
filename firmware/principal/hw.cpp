/*
 * hw.cpp - acesso ao hardware (ESP32-C3 SuperMini, placa Galileia3)
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * https://github.com/EXOTIC-TEAM-MX/EVA-RT
 * Adaptado do hw.cpp do perseguidor RUSH - Dreamcast. Feito para o
 * Arduino-ESP32 core 3.x (ledcAttach/ledcWrite por pino).
 *
 * - Barra: modulo barra.h/.cpp (copia do teste_barra validado).
 * - Motores: TB6612FNG com PWMA/PWMB em 3V3. Cada entrada (AIN1, AIN2,
 *   BIN1, BIN2) tem um canal do LEDC a 20 kHz. Para a frente: IN1 = 1 e
 *   IN2 em 0 durante a fracao 'v' do periodo (anda) e em 1 no resto
 *   (freio curto). Para tras, o contrario. PWM 0 = as duas em 1 = freio.
 * - EDF: pulso de servo (1000..2000 us a 50 Hz) num canal do LEDC no GPIO8.
 * - Receptor IR: interrupcao nas duas bordas do GPIO6; o instante de cada
 *   borda (micros) vai para os decodificadores RC5 e NEC do ir.cpp.
 */
#include "hw.h"

Barra barra;

static const uint32_t MOTOR_CHEIO = (1UL << MOTOR_RES) - 1;   // ledcWrite: 2^res - 1 = 100 %

static DecodificadorRC5  irRC5;
static DecodificadorNEC  irNEC;
static QuadroIR          irQuadro;
static volatile bool     irNovo = false;
static portMUX_TYPE      irTrava = portMUX_INITIALIZER_UNLOCKED;

// Uma borda no GPIO6 vai para os dois decodificadores (alguns us).
// O servico de interrupcao de GPIO do Arduino nao e IRAM: durante uma
// gravacao na flash (NVS) a interrupcao espera. Isso so acontece com o
// robo parado (console ou calibracao).
static void irBorda() {
  const bool nivel = digitalRead(PINO_IR) != 0;
  const uint32_t t = micros();
  portENTER_CRITICAL_ISR(&irTrava);
  if (irRC5.borda(t, nivel)) {
    irQuadro = irRC5.quadro();
    irNovo = true;
  }
  if (irNEC.borda(t, nivel)) {
    irQuadro = irNEC.quadro();
    irNovo = true;
  }
  portEXIT_CRITICAL_ISR(&irTrava);
}

void hwIniciar() {
  // EDF primeiro: o ESC ve o minimo desde ja e arma quando a SW2 ligar
  ledcAttach(PINO_ESC, ESC_HZ, ESC_RES);
  hwVentoinha(0);

  // motores: as quatro entradas em 1 (freio) antes de qualquer coisa
  const uint8_t entradas[4] = {PINO_AIN1, PINO_AIN2, PINO_BIN1, PINO_BIN2};
  for (uint8_t i = 0; i < 4; i++) {
    ledcAttach(entradas[i], MOTOR_HZ, MOTOR_RES);
  }
  hwMotores(0, 0);

  pinMode(PINO_BOTAO1, INPUT_PULLUP);
  pinMode(PINO_BOTAO2, INPUT_PULLUP);

  // barra: ajusta o ADC (12 bits, 11 dB) e as linhas S0..S3
  const PinosBarra pinos = {{PINO_S0, PINO_S1, PINO_S2, PINO_S3}, PINO_SIG};
  barra.begin(pinos, COR_LINHA_PADRAO);
  barra.configurarLeitura(LEITURA_AMOSTRAS, LEITURA_ACOMODACAO_US, LEITURA_DESCARTE != 0);

  // receptor IR: o KY-022 fica em 1 sem portadora; o pull-up segura o pino
  // se o modulo estiver desligado
  irRC5.reiniciar();
  irNEC.reiniciar();
  pinMode(PINO_IR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PINO_IR), irBorda, CHANGE);
}

// ---------------------------------------------------------------- barra

void hwLerBarra(uint8_t norm[N_SENSORES]) {
  barra.atualizar();
  for (uint8_t i = 0; i < N_SENSORES; i++) {
#if BARRA_ESPELHADA
    const uint32_t v = barra.normalizado(N_SENSORES - 1 - i);
#else
    const uint32_t v = barra.normalizado(i);
#endif
    norm[i] = (uint8_t)((v * 255UL + 500UL) / 1000UL);   // 0..1000 -> 0..255
  }
}

// ---------------------------------------------------------------- IR

bool hwIrQuadro(QuadroIR &q) {
  bool novo;
  portENTER_CRITICAL(&irTrava);
  novo = irNovo;
  if (novo) {
    q = irQuadro;
    irNovo = false;
  }
  portEXIT_CRITICAL(&irTrava);
  return novo;
}

bool hwIrNivel() {
  return digitalRead(PINO_IR) != 0;
}

// ---------------------------------------------------------------- saidas

// v: -255..255. A entrada do lado do sentido fica em 1; a outra fica em 0
// (anda) durante |v|/255 do periodo e em 1 (freio) no resto.
static void motor(uint8_t in1, uint8_t in2, int16_t v) {
  if (v > 255) v = 255;
  else if (v < -255) v = -255;
  const uint32_t mag = (uint32_t)(v < 0 ? -v : v);
  const uint32_t freio = MOTOR_CHEIO - (mag * MOTOR_CHEIO + 127UL) / 255UL;
  if (v >= 0) {
    ledcWrite(in1, MOTOR_CHEIO);
    ledcWrite(in2, freio);
  } else {
    ledcWrite(in1, freio);
    ledcWrite(in2, MOTOR_CHEIO);
  }
}

void hwMotores(int16_t esq, int16_t dir) {
#if INVERTER_MOTOR_ESQ
  esq = -esq;
#endif
#if INVERTER_MOTOR_DIR
  dir = -dir;
#endif
#if TROCAR_MOTORES
  motor(PINO_AIN1, PINO_AIN2, dir);
  motor(PINO_BIN1, PINO_BIN2, esq);
#else
  motor(PINO_AIN1, PINO_AIN2, esq);
  motor(PINO_BIN1, PINO_BIN2, dir);
#endif
}

void hwVentoinha(uint8_t v) {
  const uint32_t us = ESC_MIN_US + ((uint32_t)v * (ESC_MAX_US - ESC_MIN_US) + 127UL) / 255UL;
  const uint32_t periodoUs = 1000000UL / ESC_HZ;
  ledcWrite(PINO_ESC, (us << ESC_RES) / periodoUs);
}

void hwBipe(uint16_t ms, uint16_t hz, uint8_t volume) {
  if (volume == 0 || hz == 0) {
    delay(ms);
    return;
  }
  const uint32_t meioUs = 500000UL / hz;
  const uint32_t t0 = millis();
  bool frente = true;
  while (millis() - t0 < ms) {
    const int16_t v = frente ? (int16_t)volume : -(int16_t)volume;
    hwMotores(v, v);
    delayMicroseconds(meioUs);
    frente = !frente;
  }
  hwMotores(0, 0);
}

bool hwBotao(uint8_t n) {
  return digitalRead(n == 1 ? PINO_BOTAO1 : PINO_BOTAO2) == LOW;
}
