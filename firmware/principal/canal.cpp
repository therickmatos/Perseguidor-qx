/*
 * canal.cpp - console pela USB e pelo Bluetooth (BLE, servico UART da Nordic)
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * O servidor BLE segue o exemplo "UART" da biblioteca BLE do Arduino-ESP32 3.x.
 *
 * Fluxo:
 *  - o que chega do celular entra numa fila (tarefa do BLE) e e lido no laco
 *    principal. Antes da senha, so "senha=..." e aceito;
 *  - o que o console escreve vai para a USB e, com a senha certa, para o
 *    celular em pedacos de 20 bytes (uma notificacao cada, com uma pausa
 *    curta: mandar seguido faz a pilha do BLE descartar);
 *  - ao desconectar, a senha vale de novo e o robo volta a anunciar.
 * Durante a partida nada e escrito; o que chegar fica na fila para depois.
 */
#include "canal.h"
#include "config.h"

Canal Con;

#if BLE_CONSOLE

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define NUS_SERVICO  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   // celular -> robo
#define NUS_TX       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"   // robo -> celular
#define BLE_PEDACO   20      // bytes por notificacao (MTU minimo do BLE)
#define BLE_PAUSA_MS  8

static BLEServer         *servidor = nullptr;
static BLECharacteristic *caracTx = nullptr;
static volatile bool      conectado = false;
static volatile bool      caiu = false;      // desconectou: anunciar de novo
static volatile uint32_t  tCaiu = 0;
static volatile bool      liberado = false;  // senha certa nesta conexao

// Chegada do celular (escrita pela tarefa do BLE, lida no laco principal)
#define FILA 256
static char              rxFila[FILA];
static volatile uint16_t rxIni = 0, rxFim = 0;
static portMUX_TYPE      rxTrava = portMUX_INITIALIZER_UNLOCKED;

// Comandos ja liberados (so o laco principal mexe)
static char     cmdFila[FILA];
static uint16_t cmdIni = 0, cmdFim = 0;

// Linha da senha
static char    senhaLinha[32];
static uint8_t senhaN = 0;

// Saida para o celular
static uint8_t txBuf[BLE_PEDACO];
static uint8_t txN = 0;

class AoConectar : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    conectado = true;
  }
  void onDisconnect(BLEServer *) override {
    conectado = false;
    liberado = false;
    tCaiu = millis();
    caiu = true;
  }
};

class AoReceber : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    String v = c->getValue();
    portENTER_CRITICAL(&rxTrava);
    for (size_t i = 0; i < v.length(); i++) {
      uint16_t prox = (uint16_t)((rxFim + 1) % FILA);
      if (prox == rxIni) break;   // fila cheia: descarta o resto
      rxFila[rxFim] = v[i];
      rxFim = prox;
    }
    portEXIT_CRITICAL(&rxTrava);
  }
};

static void bleEnviar() {
  if (txN && caracTx && conectado) {
    caracTx->setValue(txBuf, txN);
    caracTx->notify();
    delay(BLE_PAUSA_MS);
  }
  txN = 0;
}

static void bleByte(uint8_t c) {
  txBuf[txN++] = c;
  if (txN >= BLE_PEDACO || c == '\n') bleEnviar();
}

static void bleTexto(const char *s) {
  while (*s) bleByte((uint8_t)*s++);
  bleEnviar();
}

void Canal::iniciar() {
  BLEDevice::init(BLE_NOME);
  servidor = BLEDevice::createServer();
  servidor->setCallbacks(new AoConectar());
  BLEService *servico = servidor->createService(NUS_SERVICO);
  caracTx = servico->createCharacteristic(NUS_TX, BLECharacteristic::PROPERTY_NOTIFY);
  caracTx->addDescriptor(new BLE2902());
  BLECharacteristic *rx = servico->createCharacteristic(
      NUS_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new AoReceber());
  servico->start();
  servidor->getAdvertising()->start();
}

void Canal::bombear() {
  if (caiu && millis() - tCaiu >= 500) {
    caiu = false;
    senhaN = 0;
    cmdIni = cmdFim = 0;
    txN = 0;
    if (servidor) servidor->startAdvertising();
  }
  for (;;) {
    char ch = 0;
    portENTER_CRITICAL(&rxTrava);
    const bool tem = rxIni != rxFim;
    if (tem) {
      ch = rxFila[rxIni];
      rxIni = (uint16_t)((rxIni + 1) % FILA);
    }
    portEXIT_CRITICAL(&rxTrava);
    if (!tem) break;

    if (liberado) {
      uint16_t prox = (uint16_t)((cmdFim + 1) % FILA);
      if (prox != cmdIni) {
        cmdFila[cmdFim] = ch;
        cmdFim = prox;
      }
    } else if (ch == '\r' || ch == '\n') {
      if (senhaN) {
        senhaLinha[senhaN] = 0;
        if (strcmp(senhaLinha, "senha=" BLE_SENHA) == 0) {
          liberado = true;
          bleTexto(FW_NOME ": console liberado (? para ajuda)\n");
        } else {
          bleTexto("mande senha=... antes dos comandos\n");
        }
        senhaN = 0;
      }
    } else if (ch != ' ' && senhaN < sizeof(senhaLinha) - 1) {
      senhaLinha[senhaN++] = ch;
    }
  }
}

int Canal::available() {
  bombear();
  return Serial.available() + (int)((cmdFim + FILA - cmdIni) % FILA);
}

int Canal::read() {
  bombear();
  if (Serial.available() > 0) return Serial.read();
  if (cmdIni == cmdFim) return -1;
  const char ch = cmdFila[cmdIni];
  cmdIni = (uint16_t)((cmdIni + 1) % FILA);
  return (uint8_t)ch;
}

int Canal::peek() {
  bombear();
  if (Serial.available() > 0) return Serial.peek();
  return cmdIni == cmdFim ? -1 : (uint8_t)cmdFila[cmdIni];
}

size_t Canal::write(uint8_t c) {
  Serial.write(c);
  if (liberado && conectado) bleByte(c);
  return 1;
}

size_t Canal::write(const uint8_t *buf, size_t n) {
  Serial.write(buf, n);
  if (liberado && conectado) {
    for (size_t i = 0; i < n; i++) bleByte(buf[i]);
  }
  return n;
}

void Canal::flush() {
  if (liberado && conectado) bleEnviar();
}

bool Canal::bleLigado() const     { return servidor != nullptr; }
bool Canal::bleConectado() const  { return conectado; }
bool Canal::bleLiberado() const   { return liberado; }

#else  // BLE_CONSOLE == 0: so a USB

void Canal::iniciar() {}
void Canal::bombear() {}
int Canal::available() { return Serial.available(); }
int Canal::read() { return Serial.read(); }
int Canal::peek() { return Serial.peek(); }
size_t Canal::write(uint8_t c) { return Serial.write(c); }
size_t Canal::write(const uint8_t *buf, size_t n) { return Serial.write(buf, n); }
void Canal::flush() {}
bool Canal::bleLigado() const { return false; }
bool Canal::bleConectado() const { return false; }
bool Canal::bleLiberado() const { return false; }

#endif
