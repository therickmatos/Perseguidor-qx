/*
 * estado.h - variaveis globais do sketch e funcoes do console
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 */
#pragma once

#include "barra.h"
#include "corrida.h"
#include "ir.h"

extern Parametros P;
extern bool       temCal;            // calibracao aceita para correr (nova ou a gravada, escolhida no botao 2)
extern uint8_t    nivelAtual;
extern Corrida    corrida;
extern uint32_t   cicloMaxUs;        // maior tempo de um ciclo na ultima partida
extern uint32_t   ciclosAtrasados;   // ciclos que passaram de PERIODO_US
extern uint8_t    arena;             // numero da arena do modulo de inicio (par)
extern bool       arenaGravada;      // o juiz (ou o console) ja gravou um numero

// principal.ino
RegrasIR regrasIR();
void bipe(uint8_t vezes, uint16_t ms, uint16_t hz);   // 'vezes' bipes de 'ms', com pausa igual

// console.cpp (so e chamado com o robo parado)
void consoleProcessar();
void consoleEstado();
void consoleNivel();
void consoleResumo();
void consoleArena();
void consoleCalibracao(const ResultadoCalibracao &r);
void consoleTempoLeitura();
void testeMotores();
void testeEdf();
