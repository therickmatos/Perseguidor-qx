/*
 * params.h - parametros ajustaveis entre partidas (console serial + NVS)
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * Adaptado do firmware do perseguidor RUSH - Dreamcast.
 *
 * Os valores de controle partem dos do EVA-RT (SPEED 40, KP 0.06), como no
 * Dreamcast. Os da EDF sao um chute baixo: subir na pista.
 * Sem dependencia do Arduino: tambem compila no PC (firmware/testes).
 */
#pragma once

#include <stdint.h>

struct Parametros {
  // ---- PD (erro em milesimos de sensor: +-3500, 1000 = 9,525 mm na QRE-8D)
  float    kp;        // PWM por unidade de erro
  float    kd;        // PWM por (unidade de erro por ms)
  uint8_t  fd;        // filtro da derivada, % da leitura nova (100 = sem filtro)

  // ---- velocidade (PWM 0..255)
  uint8_t  vel;       // base nas curvas, no nivel 0 (SPEED do EVA-RT)
  uint8_t  reta;      // quanto a mais nas retas (0 = velocidade unica)
  uint8_t  passo;     // aumento de 'vel' por nivel (botao 2)
  uint8_t  vent;      // EDF no nivel 0 (0..255 = 0..100 % do curso do ESC)
  uint8_t  vpasso;    // aumento da EDF por nivel
  uint8_t  nivel;     // nivel ao ligar

  // ---- reta x curva
  // A curvatura e estimada pelo comando medio de direcao em % do PWM base
  // (diferenca entre as rodas / velocidade): 0 = reta, ~55 = curva de R100.
  uint16_t ecurva;    // |erro| acima disso e entrada de curva: volta para 'vel' na hora
  uint16_t treta;     // ms com o erro abaixo de ecurva/2 para poder passar de 'vel'
  uint8_t  rreta;     // curvatura (%) ate a qual anda em vel + reta
  uint8_t  rcurva;    // curvatura (%) a partir da qual anda em 'vel'; entre as
                      //   duas a velocidade e proporcional (rcurva <= rreta: degrau)
  uint16_t acel;      // PWM por segundo ao acelerar (tambem e a rampa da largada)
  uint8_t  fcurva;    // freada ao baixar a velocidade: % da queda aplicada a mais
  uint16_t tfcurva;   //   por um instante (constante de tempo, ms); 0 % = sem freada

  uint8_t  re;        // re maxima da roda de dentro (PWM)

  // ---- seguranca
  uint16_t perdido;   // ms sem ver a linha ate desligar
  uint16_t tmax;      // s: desliga se a partida passar disso (0 = sem limite).
                      //   A partida tem 2 min e o juiz pode estender (regra 6.3)

  // ---- parada (comando de parada do modulo de inicio)
  uint16_t freio;     // re para frear, em % do PWM base do momento
  uint16_t tfreio;    // ms de re (voltou para tras: diminua; demorou a parar: aumente)

  // ---- controle NEC (o do kit do KY-022). Outro controle igual por perto,
  //      com as mesmas teclas, tambem larga e para o robo.
  uint8_t  nec;       // 1 = aceita o controle NEC
  uint16_t necend;    // endereco do controle (o comando 'ir' mostra)
  uint8_t  necpart;   // tecla de partida
  uint8_t  necpara;   // tecla de parada

  // ---- avisos
  uint8_t  bipe;      // volume do bipe nos motores (PWM; 0 = mudo)
};

inline void parametrosPadrao(Parametros &p) {
  // kp, kd e ecurva: os da barra V1 (0,06 / 0,40 / 600, com 1000 = 7,8 mm)
  // convertidos para o passo da QRE-8D (x 9,525 / 7,8), o mesmo PWM por mm
  p.kp = 0.0733f;     // KP 0,06 do EVA-RT
  p.kd = 0.4885f;     // KD 0.6 do EVA-RT era por volta do laco (~0,65 ms)
  p.fd = 50;

  // Primeiros testes na pista (28/11/2026): motores acima da tensao nominal
  // (bateria 3S) e PWM 40 nao vencia o atrito no teste das rodas (18/11).
  // Sai em 60, sem velocidade de reta; o botao 2 em "pronto" soma 10 por
  // toque (segurar volta ao nivel 0). A EDF fica no minimo e nao sobe com o
  // nivel. Referencia do EVA-RT: vel 40, reta 10, passo 5, vent 100, vpasso 10.
  p.vel = 60;
  p.reta = 0;
  p.passo = 10;
  p.vent = 100;       // ~1390 us: girou com a bateria nova (28/11); 50 nao foi testado
  p.vpasso = 0;       // o nivel so muda as rodas
  p.nivel = 0;

  p.ecurva = 491;     // 4,7 mm
  p.treta = 60;
  p.rreta = 15;
  p.rcurva = 40;
  p.acel = 300;
  p.fcurva = 100;
  p.tfcurva = 40;

  p.re = 255;         // como no EVA-RT (-255)

  p.perdido = 100;    // EVA-RT: 10 ms
  p.tmax = 180;       // 2 min de partida + acrescimo do juiz

  p.freio = 100;      // EVA-RT: -SPEED por 75 ms
  p.tfreio = 75;

  // 0x45 e 0x46 sao as duas primeiras teclas da linha de cima nos dois
  // controles comuns do kit: "1" e "2" no de 17 teclas, "CH-" e "CH" no de 21.
  p.nec = 1;
  p.necend = 0x00;
  p.necpart = 0x45;
  p.necpara = 0x46;

  p.bipe = 40;
}

// Velocidades de um nivel (o botao 2 sobe o nivel, como o +5 do EVA-RT)
uint8_t velCurvaDoNivel(const Parametros &p, uint8_t nivel);
uint8_t velRetaDoNivel(const Parametros &p, uint8_t nivel);
uint8_t ventDoNivel(const Parametros &p, uint8_t nivel);
uint8_t nivelMaximo(const Parametros &p);
