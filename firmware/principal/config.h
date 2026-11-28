/*
 * config.h - configuracao de compilacao do Perseguidor-qx (pinos, barra, IR, motores, EDF)
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * https://github.com/EXOTIC-TEAM-MX/EVA-RT
 * Adaptado do firmware do perseguidor RUSH - Dreamcast (therickmatos/Perseguidor-de-linha),
 * que veio do seguidor RUSH - SATURN (therickmatos/Seguidor-de-linha).
 *
 * O que muda de robo para robo ou de pista para pista fica aqui.
 * O que se ajusta entre partidas (ganhos, velocidades, EDF) fica em params.h
 * e pode ser alterado pelo console serial, sem regravar.
 */
#pragma once

#define FW_NOME    "Perseguidor-qx"
#define FW_VERSAO  "0.1.0"

// ------------------------------------------------------------------ pista
// RoboCore/TRIF e a pista do laboratorio: linha branca em fundo preto.
// O robo sempre liga com esta cor: a cor gravada na NVS (pelo 'l' do
// teste_barra) e ignorada. O comando 'cor' do console troca so ate desligar.
#define COR_LINHA_PADRAO  LINHA_BRANCA

// ------------------------------------------------------------------ pinos
// Placa Galileia3 com ESP32-C3 SuperMini (conferido na netlist do .epro):
//   GPIO0..3  S0..S3 do 74HC4067 (GPIO2 tem pull-up de 10k, R3: strapping)
//             (desde 28/11 o mux e um modulo CD74HC4067 ligado no conector
//             da barra, U5, com a QRE-8D nos canais C0..C7)
//   GPIO4     SIG, saida comum do mux (ADC1_CH4)
//   GPIO10    AIN1  \  TB6612, motor A = esquerdo (AO1/AO2 = M_ESQ_A/B)
//   GPIO20    AIN2  /  (PWMA e PWMB ligados no 3V3: o PWM vai nas entradas)
//   GPIO5     BIN1  \  motor B = direito (BO1/BO2 = M_DIR_A/B)
//   GPIO7     BIN2  /
//   GPIO8     ESC_SW: sinal do ESC da EDF, passa pela chave SW2 (R4 de 10k
//             puxa o ESC_SIG para o GND). E tambem o LED azul da SuperMini.
//   GPIO6     START: receptor IR KY-022
//   GPIO21    BTN: botao da placa (botao 1)
//   GPIO9     BOOT da SuperMini (botao 2)
// Sem divisor para a bateria: nao ha medida de tensao nesta placa.
#define PINO_S0      0
#define PINO_S1      1
#define PINO_S2      2
#define PINO_S3      3
#define PINO_SIG     4
#define PINO_AIN1   10
#define PINO_AIN2   20
#define PINO_BIN1    5
#define PINO_BIN2    7
#define PINO_ESC     8
#define PINO_IR      6
#define PINO_BOTAO1 21
#define PINO_BOTAO2  9

// ------------------------------------------------------------------ barra
// A leitura e a calibracao sao do modulo barra.h/.cpp (copia identica do
// firmware/teste_barra, validado na bancada). O mapa do mux (MAPA_PADRAO)
// fica la. Aqui so o que o firmware do robo usa.
// QRE-8D da RoboCore: 8 sensores em linha reta, 9,525 mm entre eles
// (a barra V1 tinha 12, a 7,8 mm, em arco)
#define N_SENSORES    8

// Leitura de cada sensor: amostras, acomodacao do mux (us) e descarte da
// primeira conversao. Comecam nos valores do barra.h; troque pelos que a
// varredura 't' do teste_barra recomendar e confira o tempo no console ('?').
#define LEITURA_AMOSTRAS        1   // 4 dava 2053 us (> ciclo de 2000) no robo, 18/11
#define LEITURA_ACOMODACAO_US  20
#define LEITURA_DESCARTE        1

// 1 = sem calibracao (teste rapido na pista): liga direto em "pronto" e usa
// a leitura crua em escala cheia (preto ~4095, branco 330..2000 na QRE-8D com
// J2 fechado e pull-ups de 10k). Botao 1 larga em 1 s; o controle larga na hora.
// 0 = fluxo normal (botao 1 calibra, botao 2 usa a calibracao gravada).
#define SEM_CALIBRACAO       1

// Largada automatica (teste, enquanto o botao 1 nao funciona): em "pronto",
// com a barra vendo a linha sem parar por LARGADA_AUTO_MS, o robo larga
// (2 bipes curtos de aviso antes). Depois de uma partida, so arma de novo
// quando a linha some por 0,5 s (robo levantado). 0 = desligada.
#define LARGADA_AUTO         1
#define LARGADA_AUTO_MS   3000

// 1 = a barra esta montada ao contrario (o S0 do mapa ficou na direita)
#define BARRA_ESPELHADA      0

// Limiares do controle na escala 0..255 (255 = em cima da linha). A barra
// entrega 0..1000 e o hw.cpp converte. (Nomes diferentes dos do barra.h.)
#define NORM_RUIDO           13   // abaixo disso o sensor nao entra na media
#define NORM_LINHA           77   // algum sensor acima disso = linha vista

// A linha de 19 mm cobre 2 sensores (passo de 9,525 mm), 3 se o robo
// estiver um pouco torto. LARGURA_FAIXA ou mais sensores vizinhos ativos
// e uma faixa larga; LARGURA_CRUZAMENTO ou mais e uma faixa atravessada.
// No perseguidor a unica faixa atravessada e a linha de partida (regra 4.8:
// reta de 200 mm cruzando a pista, com 250 mm de reta antes e depois), que
// cobre a barra inteira.
//
// Numa curva fechada a barra, que vai uns 100 mm a frente do eixo, fica
// quase paralela a linha e tambem ve uma faixa larga. Por isso a faixa so
// e tratada como atravessada (erro mantido) se o robo nao estiver virando:
// comando medio de direcao abaixo de CRUZ_COMANDO_MAX % da velocidade de
// curva. Com LARGURA_TOTAL sensores e faixa atravessada em qualquer situacao.
// Com a QRE-8D (8 sensores, 9,525 mm): faixa larga a partir de 5 vizinhos
// (48 mm), atravessada com 7, e com os 8 em qualquer situacao. Conferido no
// simulador (firmware/testes). Na barra V1 (12 sensores) eram 5, 8 e 10.
#define LARGURA_FAIXA         5
#define LARGURA_CRUZAMENTO    7
#define LARGURA_TOTAL         8
#define CRUZ_COMANDO_MAX     30
#define FAIXA_MAX_MS         80   // erro mantido no maximo por este tempo

// Num trecho reto a linha nao anda mais que ~4 mm por ms (400 em 1 ms; 1000
// = 9,525 mm na QRE-8D). Um
// salto maior so e aceito se durar SALTO_CICLOS: da tempo de a faixa
// atravessada, que entra por um lado da barra, ser reconhecida.
#define SALTO_MAX           (400L * PERIODO_US / 1000L)
#define SALTO_CICLOS          3

// Perdeu a linha com o erro alem disso (1000 = um sensor; o extremo e 3500): ela saiu pela
// lateral e o erro vai para o extremo. Senao (falha no meio) o erro e mantido.
#define LIMIAR_LATERAL     2000

// ------------------------------------------------------------------ modulo de inicio (IR)
// Igual ao do Dreamcast. Regras 3.1.1 e 6.4 do Perseguidor (RoboCore,
// 12/12/2023): o robo liga e para pelo modulo de partida, sem tempo de
// seguranca. Item 8.2.3: vale receptor IR proprio no padrao de um dos
// modulos recomendados.
// RC5 do startmodule.com (conferido no codigo oficial, github.com/p1rse/
// robot-sumo-start-module):
//   programar: endereco 0x0B, comando = numero da arena (par)
//   partida:   endereco 0x07, comando = numero da arena + 1
//   parada:    endereco 0x07, comando = numero da arena
// No TRIF o controle fica com o participante e o controle NEC do kit do
// KY-022 foi aceito em 2024 e 2025: o NEC vem ligado (teclas em params.h).
#define IR_END_PROGRAMA    0x0B
#define IR_END_PARTIDA     0x07
// 1 = para com o comando de parada de qualquer arena; 0 = so com o numero gravado
#define IR_PARADA_QUALQUER_ID  0

// ------------------------------------------------------------------ motores (TB6612FNG)
// PWMA/PWMB fixos em 3V3: a velocidade vai em AIN1/AIN2 e BIN1/BIN2, no modo
// "anda/freia" (uma entrada em 1, a outra em PWM invertido), o mesmo
// comportamento do PH/EN do DRV8874 no Dreamcast. PWM 0 = freio (curto).
#define MOTOR_HZ          20000   // o TB6612 aceita ate 100 kHz
#define MOTOR_RES            10   // bits do LEDC (80 MHz / 20 kHz = 4000 > 1024)

// Motor A = esquerdo, motor B = direito. Se uma roda girar ao contrario no
// teste de motores, mude aqui (ou inverta os fios).
#define INVERTER_MOTOR_ESQ   0
#define INVERTER_MOTOR_DIR   0
#define TROCAR_MOTORES       0    // 1 = o motor A esta na direita

// ------------------------------------------------------------------ EDF (ESC)
// Pulso de servo no GPIO8. Com a SW2 desligada o ESC nao recebe nada. O
// firmware manda o minimo (EDF parada) o tempo todo fora da partida: assim
// o ESC arma sozinho quando a SW2 e ligada.
#define ESC_HZ               50   // padrao de servo; muitos ESC aceitam ate 400
#define ESC_RES              14   // bits do LEDC (20 ms / 16384 = 1,2 us)
#define ESC_MIN_US         1000   // EDF parada (armado)
#define ESC_MAX_US         2000   // EDF no maximo

// ------------------------------------------------------------------ ciclo
// Periodo fixo do controle (multiplo de 1000 us). A leitura dos sensores
// tem de caber com folga: o console mostra o tempo medido ('?') e, depois da
// partida, o maior ciclo e os atrasados ('res'). O Dreamcast (ATmega) roda a
// 1000 us; aqui comeca em 2000 por causa do ADC do ESP32-C3 com 4 amostras.
#ifndef PERIODO_US                // (o simulador troca pela linha de comando)
#define PERIODO_US         2000
#endif

// ------------------------------------------------------------------ largada e parada
// Pelo modulo de inicio a saida e imediata (regra 3.1.1). Pelo botao 1, so
// para teste, o robo espera LARGADA_BOTAO_MS para a mao sair.
#define LARGADA_BOTAO_MS   1000
#define VENT_SUBIDA_MS      200   // rampa da EDF (as rodas saem junto, em rampa de 'acel')
#define VENT_POS_PARADA_MS  500   // EDF ligada depois de frear
#define VENT_MAXIMO         255   // teto da EDF ao subir de nivel
#define VEL_MAXIMA          140   // teto do PWM base ao subir de nivel (EVA-RT)

// ------------------------------------------------------------------ bipe
// Sem LED livre na placa, os avisos saem como bipe nos motores: as rodas
// alternam de sentido na frequencia do som (media zero, o robo nao anda).
// Volume no parametro 'bipe' (console).
#define BIPE_AGUDO         2400   // Hz
#define BIPE_GRAVE         1200

// ------------------------------------------------------------------ Bluetooth (console no celular)
// O mesmo console da USB, pelo BLE (servico UART da Nordic). Apps: Android
// "Serial Bluetooth Terminal" (aba Bluetooth LE), iOS "Bluefruit Connect" (UART).
// Pelo BLE, os comandos so valem depois de "senha=<BLE_SENHA>" em cada
// conexao: sem isso, qualquer celular por perto mandaria 'motores' ou 'edf'.
// TROQUE A SENHA antes de levar o robo para a competicao.
#define BLE_CONSOLE           1      // 0 = so USB (o firmware fica bem menor)
#define BLE_NOME   "Perseguidor-qx"
#define BLE_SENHA  "1234"

// ------------------------------------------------------------------ registro
#define LOG_TAMANHO         120   // amostras guardadas (as ultimas)
#define LOG_PERIODO_MS       20   // 120 x 20 ms = ultimos 2,4 s
