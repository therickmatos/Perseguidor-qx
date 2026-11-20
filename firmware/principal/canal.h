/*
 * canal.h - console pela USB e pelo Bluetooth (BLE) ao mesmo tempo
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 *
 * O ESP32-C3 so tem BLE (nao o Bluetooth classico do HC-05). O console usa o
 * servico "UART" da Nordic (NUS), que os apps de terminal BLE entendem:
 * Android "Serial Bluetooth Terminal", iOS "Bluefruit Connect" (UART).
 *
 * 'Con' substitui o Serial no console: o que se escreve vai para a USB e
 * para o celular conectado; o que chega dos dois vira comando. Pelo BLE, os
 * comandos so valem depois de "senha=<BLE_SENHA>" (config.h), em cada conexao.
 */
#pragma once

#include <Arduino.h>

class Canal : public Stream {
public:
  void iniciar();                       // liga o BLE (se BLE_CONSOLE = 1)

  int available() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buf, size_t n) override;
  void flush() override;                // manda ja o que estiver esperando para o BLE

  bool bleLigado() const;
  bool bleConectado() const;
  bool bleLiberado() const;             // senha certa nesta conexao

private:
  void bombear();                       // move o que chegou do BLE para a fila de comandos
};

extern Canal Con;
