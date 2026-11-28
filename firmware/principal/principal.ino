/*
  principal - firmware de competicao do Perseguidor-qx
  (placa Galileia3, ESP32-C3 SuperMini, Arduino-ESP32 core 3.x)

  Baseado no EVA-RT v1.2.2 (c) 2025 EXOTIC TEAM MX
  https://github.com/EXOTIC-TEAM-MX/EVA-RT
  Adaptado do firmware do perseguidor RUSH - Dreamcast
  (therickmatos/Perseguidor-de-linha): o mesmo controle e o mesmo modulo de
  inicio por IR, com a leitura da barra do teste_barra (validado na bancada).

      DRIVER:             TB6612FNG (PWM nas entradas, PWMA/PWMB em 3V3)
      SENSORES:           QRE-8D (8 QRE1113) + modulo CD74HC4067 (barra.h/.cpp)
      SUCCAO:             EDF QX-Motor 30 mm por ESC (GPIO8, chave SW2)
      MODULO DE INICIO:   receptor IR KY-022 no GPIO6: controle NEC do kit
                          (o que vale no TRIF) e padrao startmodule.com (RC5)
      CONTROLE:           PD, ciclo fixo de PERIODO_US (2 ms)

  Placa na IDE: "ESP32C3 Dev Module" (ou "Nologo ESP32C3 Super Mini"), com
  Ferramentas > USB CDC On Boot: Enabled. Gravar com SW1 e SW2 desligadas.

  Uso (botao 1 = BTN da placa, botao 2 = BOOT da SuperMini, controle IR).
  Sem LED livre: os avisos sao bipes nos motores (ver firmware/README.md).
    esperando       botao 1 = calibrar | botao 2 = usar a calibracao gravada
                    segurar 1 = teste das rodas | segurar 2 = teste da EDF
    calibrando      passar a barra inteira pela linha; botao 1 termina
    pronto          controle: partida = sai na hora | programar = grava a arena
                    botao 1 = sai em 1 s, SO PARA TESTE (segurar = calibrar de novo)
                    botao 2 = sobe um nivel (segurar = nivel 0)
    correndo        controle: parada = freia e volta para "pronto"
                    qualquer botao desliga
    fim (sem o comando de parada)
                    botao ou parada = "pronto" | partida = sai de novo

  Arquivos:
    config.h      pinos, barra, IR, motores, EDF e limiares (compilacao)
    params.h      ganhos, velocidades e EDF (console serial, NVS)
    barra.*       leitura e calibracao da barra (copia do teste_barra)
    linha.*       posicao da linha
    corrida.*     largada, PD, reta x curva, parada
    ir.*          decodificadores RC5 e NEC e comandos do modulo de inicio
    hw.*          barra, motores, EDF, IR, botoes, bipe
    memoria.*     parametros e arena na NVS
    console.cpp   comandos pela serial

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "params.h"
#include "linha.h"
#include "corrida.h"
#include "ir.h"
#include "hw.h"
#include "memoria.h"
#include "estado.h"
#include "canal.h"

#if !ARDUINO_USB_CDC_ON_BOOT
#error "Ative Ferramentas > USB CDC On Boot: Enabled (arduino-cli: --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc)"
#endif

Parametros P;
bool       temCal = false;
uint8_t    nivelAtual = 0;
Corrida    corrida;
uint32_t   cicloMaxUs = 0;
uint32_t   ciclosAtrasados = 0;
uint8_t    arena = 0;
bool       arenaGravada = false;

#define TOQUE_LONGO     0x80
#define TOQUE_LONGO_MS  800

// O controle repete o quadro a cada ~110 ms enquanto o botao fica apertado,
// com o mesmo bit T. Repeticoes mais proximas que isto sao ignoradas.
#define IR_REPETICAO_MS 250

// Partida pedida pelo controle na tela de fim: sai direto, sem esperar outra
static bool partidaPendente = false;

// ---------------------------------------------------------------- bipes e botoes

void bipe(uint8_t vezes, uint16_t ms, uint16_t hz) {
  for (uint8_t i = 0; i < vezes; i++) {
    hwBipe(ms, hz, P.bipe);
    if (i + 1 < vezes) delay(ms);
  }
}

static void bipeOk()   { bipe(2, 70, BIPE_AGUDO); }
static void bipeErro() { bipe(1, 500, BIPE_GRAVE); }

// 0 = nada; 1 ou 2 = toque no botao; com TOQUE_LONGO se foi segurado.
// Devolve quando o botao e solto.
static uint8_t lerToque() {
  uint8_t b = hwBotao(1) ? 1 : (hwBotao(2) ? 2 : 0);
  if (!b) return 0;
  delay(20);
  if (!hwBotao(b)) return 0;

  uint32_t t0 = millis();
  bool longo = false;
  while (hwBotao(b)) {
    if (!longo && millis() - t0 >= TOQUE_LONGO_MS) {
      longo = true;
      bipe(1, 40, BIPE_AGUDO);   // avisa que ja pode soltar
    }
    delay(1);
  }
  delay(30);
  return longo ? (b | TOQUE_LONGO) : b;
}

// ---------------------------------------------------------------- modulo de inicio

static QuadroIR irUltimo;
static uint32_t irUltimoMs = 0;
static bool     irTemUltimo = false;

RegrasIR regrasIR() {
  RegrasIR r;
  r.arena = arena;
  r.paradaQualquer = IR_PARADA_QUALQUER_ID;
  r.endPrograma = IR_END_PROGRAMA;
  r.endPartida = IR_END_PARTIDA;
  r.nec = P.nec;
  r.necEndereco = P.necend;
  r.necPartida = P.necpart;
  r.necParada = P.necpara;
  return r;
}

// Le o quadro mais novo (se houver) e diz o que ele e para este robo.
// Repeticao do mesmo aperto (mesmo T) e descartada. Nao escreve na serial:
// tambem e chamada durante a partida.
static ComandoIR lerIR(QuadroIR &q) {
  if (!hwIrQuadro(q)) return IR_NADA;
  const uint32_t agora = millis();
  const bool repeticao = irTemUltimo && q.protocolo == irUltimo.protocolo &&
                         q.endereco == irUltimo.endereco && q.comando == irUltimo.comando &&
                         q.toggle == irUltimo.toggle && (agora - irUltimoMs) < IR_REPETICAO_MS;
  irUltimo = q;
  irUltimoMs = agora;
  irTemUltimo = true;
  if (repeticao) return IR_NADA;
  return interpretarIR(q, regrasIR());
}

// Com o robo parado: le, avisa na serial e ja grava o numero da arena.
static ComandoIR tratarIR() {
  QuadroIR q;
  ComandoIR c = lerIR(q);
  if (c == IR_NADA) return c;
  if (c == IR_PROGRAMA) {
    arena = arenaDoQuadro(q);
    arenaGravada = true;
    memSalvarArena(arena);
    consoleArena();
    bipe(3, 60, BIPE_AGUDO);
  } else if (c == IR_OUTRO) {
    // sem bipe: pode ser o controle da equipe adversaria
    Con.printf("IR %s: endereco %u comando %u (nao e para este robo)\n",
                  q.protocolo == IR_NEC ? "NEC" : "RC5", (unsigned)q.endereco, (unsigned)q.comando);
  }
  return c;
}

// ---------------------------------------------------------------- calibracao

// Minimo e maximo de cada sensor (barra.cpp): tem que passar a barra inteira pela linha.
static bool calibrar() {
  Con.println(F("calibrando: passe a barra inteira pela linha; botao 1 termina"));
  bipe(1, 60, BIPE_AGUDO);
  barra.iniciarCalibracao();
  while (!hwBotao(1)) barra.amostrarCalibracao();
  while (hwBotao(1)) delay(1);
  delay(30);

  ResultadoCalibracao r = barra.finalizarCalibracao();
  consoleCalibracao(r);
  if (!r.aceita) {
    Con.println(r.manteveAnterior ? F("REJEITADA: a calibracao anterior continua valendo")
                                     : F("REJEITADA: refaca"));
    Con.println(F("se forem sempre os mesmos sensores, confira os canais com o comando m"));
    bipeErro();
    return false;
  }
  temCal = true;
  Con.println(barra.salvarCalibracao() ? F("calibracao aceita e gravada")
                                          : F("calibracao aceita, mas ERRO ao gravar na NVS"));
  bipeOk();
  return true;
}

// ---------------------------------------------------------------- telas

static void telaEspera() {
  Con.println(F("botao 1 = calibrar | botao 2 = usar a calibracao gravada | "
                   "segurar 1 = teste das rodas | segurar 2 = teste da EDF"));
  while (!temCal) {
    consoleProcessar();
    if (tratarIR() == IR_PARTIDA) {
      Con.println(F("partida ignorada: calibre antes"));
      bipeErro();
    }
    uint8_t t = lerToque();
    if (t == 1) {
      calibrar();
    } else if (t == 2) {
      if (barra.calibrado()) {
        temCal = true;
        Con.println(F("calibracao gravada em uso"));
        bipeOk();
      } else {
        Con.println(F("nao ha calibracao gravada para esta barra"));
        bipeErro();
      }
    } else if (t == (1 | TOQUE_LONGO)) {
      testeMotores();
    } else if (t == (2 | TOQUE_LONGO)) {
      testeEdf();
    }
    delay(1);
  }
}

// Acompanha a barra ate o controle (ou o botao 1, para teste) mandar largar.
// Devolve a espera ate as rodas sairem, em ms.
static uint16_t telaPronto() {
  uint8_t norm[N_SENSORES];

  corrida.preparar();
  if (partidaPendente) {
    partidaPendente = false;
    return 0;
  }
  consoleNivel();
  consoleArena();
  Con.println(F("pronto: controle = partida | botao 1 = largar em 1 s (teste) | botao 2 = sobe o nivel"));
  bipe(1, 150, BIPE_AGUDO);

  for (;;) {
    consoleProcessar();
    if (tratarIR() == IR_PARTIDA) return 0;

    hwLerBarra(norm);
    corrida.observar(norm);

    uint8_t t = lerToque();
    if (t == 1) return LARGADA_BOTAO_MS;
    if (t == (1 | TOQUE_LONGO)) {
      calibrar();
      corrida.preparar();
    } else if (t == 2) {
      if (nivelAtual < nivelMaximo(P)) {
        nivelAtual++;
        bipe(1, 60, BIPE_AGUDO);
      } else {
        bipe(3, 40, BIPE_GRAVE);   // ja esta no ultimo
      }
      consoleNivel();
    } else if (t == (2 | TOQUE_LONGO)) {
      nivelAtual = 0;
      bipe(1, 300, BIPE_GRAVE);
      consoleNivel();
    }
    delay(1);
  }
}

static void correr(uint16_t esperaMs) {
  uint8_t norm[N_SENSORES];
  Saida s;
  QuadroIR q;
  uint8_t botao = 0;

  cicloMaxUs = 0;
  ciclosAtrasados = 0;
  corrida.iniciar(P, nivelAtual, esperaMs);

  uint32_t proximo = micros();
  while (corrida.fase() != FASE_FIM) {
    // ---- periodo fixo (espera ativa: o vigia da tarefa ociosa foi
    // desligado no setup; nada mais roda durante a partida)
    while ((int32_t)(micros() - proximo) < 0) {
    }
    uint32_t inicio = micros();
    if (inicio - proximo > PERIODO_US) {   // perdeu o passo: recomeca daqui
      ciclosAtrasados++;
      proximo = inicio;
    }
    proximo += PERIODO_US;

    // ---- le, decide, aplica
    hwLerBarra(norm);
    if (hwBotao(1) || hwBotao(2)) {
      if (++botao >= 5) corrida.abortar();
    } else {
      botao = 0;
    }
    // so a parada interessa aqui; programar fica para o robo parado
    if (lerIR(q) == IR_PARADA) corrida.parar();

    // 256: sem compensacao pela bateria (a placa nao mede a tensao)
    corrida.passo(norm, 256, s);
    hwMotores(s.esq, s.dir);
    hwVentoinha(s.vent);

    uint32_t gasto = micros() - inicio;
    if (gasto > cicloMaxUs) cicloMaxUs = gasto;
  }
  hwMotores(0, 0);
  hwVentoinha(0);
}

static void telaFim() {
  uint8_t motivo = corrida.resumo().motivo;
  consoleResumo();
  while (hwBotao(1) || hwBotao(2)) delay(1);   // o botao que desligou ainda apertado
  delay(30);

  // Parado pelo juiz: volta sozinho para "pronto", para o proximo round
  if (motivo == FIM_PARADA) return;

  // 2 bipes longos = perdeu a linha; 3 = botao ou tempo
  bipe(motivo == FIM_PERDEU ? 2 : 3, 300, BIPE_GRAVE);
  for (;;) {
    consoleProcessar();
    ComandoIR c = tratarIR();
    if (c == IR_PARADA) break;
    if (c == IR_PARTIDA) {
      partidaPendente = true;
      break;
    }
    if (lerToque()) break;
    delay(1);
  }
}

// ---------------------------------------------------------------- setup / loop

void setup() {
  hwIniciar();   // EDF no minimo e motores freados antes de tudo

  // USB CDC: buffer maior e sem espera, para nada travar quando ninguem le
  // a porta (robo na pista, sem cabo). Nao chamar Serial.flush().
  Serial.setTxBufferSize(4096);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);
  Con.iniciar();   // Bluetooth (BLE_CONSOLE no config.h)

  // O ciclo de controle e a calibracao sao lacos longos sem pausa, de
  // proposito: a tarefa ociosa nao roda e o vigia dela reclamaria.
  disableCore0WDT();

  if (!memCarregarParametros(P)) parametrosPadrao(P);
  nivelAtual = P.nivel;
  if (nivelAtual > nivelMaximo(P)) nivelAtual = nivelMaximo(P);
  arenaGravada = memCarregarArena(arena);
  if (!arenaGravada) arena = 0;

  EstadoNVS nvs = barra.carregarNVS();
  Con.printf("\nNVS da barra: calibracao %s\n",
                nvs.calibracao ? "carregada (botao 2 para usar)"
                               : (nvs.calibracaoInvalida ? "ignorada (outro mapa ou dados invalidos)" : "nenhuma"));
  // A cor e sempre a do config.h: a que o teste_barra gravou na NVS nao vale aqui
  if (nvs.cor && barra.corLinha() != COR_LINHA_PADRAO) {
    Con.println(F("cor da linha gravada pelo teste_barra ignorada: vale a do config.h"));
  }
  barra.setCorLinha(COR_LINHA_PADRAO);
  consoleEstado();

  // leitura lenta demais para o ciclo: 3 bipes graves (o console explica)
  bipe(1, 100, BIPE_AGUDO);
  if (barra.tempoLeituraUs() * 10 > (uint32_t)PERIODO_US * 7) {
    delay(300);
    bipe(3, 150, BIPE_GRAVE);
  }
}

void loop() {
  if (!temCal) telaEspera();
  uint16_t espera = telaPronto();
  correr(espera);
  telaFim();
}
