# Testes

Registro do que foi medido no robô. Preencher as lacunas (`___`) e datar.

## QRE-8D + CD74HC4067 (bancada, desde 28/11/2026)
Firmware: [`firmware/teste_barra/`](../firmware/teste_barra/) (já com 8 sensores em C0..C7). Ligação em [`eletronica/README.md`](../eletronica/README.md#barra-de-sensores-qre-8d--módulo-cd74hc4067-desde-28112026).

| Item | Resultado |
|---|---|
| Pull-up usado | ___ kΩ |
| J2 (3V3 Bypass) fechado? | ___ |
| `m`: canais ativos | ___ (esperado: C0..C7) |
| `d` com a barra a 3 mm, sensor no branco / no preto | ___ mV / ___ mV (preto saturado é normal) |
| `c`: menor delta | ___ (S__) — tem de passar de 150 |
| `t`: recomendado | amostras ___, acomodação ___ µs, descarte ___; t_med ___ µs |
| Firmware principal, `?` | leitura ___ µs (< 1400 µs) |

## Barra de sensores V1 (bancada; não montada no robô)
Firmware: [`firmware/teste_barra/`](../firmware/teste_barra/) (o mesmo `barra.h/.cpp` vai no firmware principal).

- **Validada na bancada** com ESP32-C3 em protoboard (pinos S0..S3 = GPIO5, 6, 7, 10; SIG = GPIO3). Os resultados não estavam nos arquivos recebidos; colar aqui a saída dos comandos:

| Comando | Resultado |
|---|---|
| `m` (canais do mux) | canais ativos: ___ — confere com o `MAPA_PADRAO` {4..15}? ___ |
| `c` (calibração) | menor delta: ___ (S__), saturados: ___ |
| `t` (varredura de tempo) | recomendado: amostras ___, acomodação ___ µs, descarte ___; t_med ___ µs, t_max ___ µs |
| `d` (diagnóstico) | maior tensão em mV: ___ (margem até 2500 mV: ___) |

- [ ] Repetir `m` e `c` com a barra ligada na placa Galiléia3 (S0..S3 = GPIO0..3, SIG = GPIO4: o `teste_barra.ino` deste repositório já está com esses pinos).
- [ ] Passar os valores do `t` para `LEITURA_*` em `firmware/principal/config.h` e conferir o tempo no `?` do firmware principal (tem de ficar abaixo de 70 % do `PERIODO_US`).

## Primeira energização (placa Galiléia3)
Fazer nesta ordem, com o multímetro, **sem a SuperMini, a barra e o KY-022 na placa** até o passo 4.

| # | Passo | Esperado | Medido |
|---|---|---|---|
| 1 | Continuidade VBAT–GND e 3V3–GND com tudo desligado | sem curto | ___ |
| 2 | Bateria no XT60, SW1 desligada: VBAT no C3 | tensão da bateria | ___ V |
| 3 | SW1 ligada: saída do Mini360 (rede 3V3) | 3,30 V (ajustar antes, `eletronica/README.md`) | ___ V |
| 4 | SW1 desligada, colocar a SuperMini, a barra e o KY-022 | — | — |
| 5 | SW1 ligada: 3V3 com a carga | 3,2–3,4 V | ___ V |
| 6 | Corrente da bateria parado (SW2 desligada) | ___ | ___ mA |
| 7 | SW1 desligada, USB no PC, gravar o `teste_barra` (SW2 desligada) | grava sem erro | ___ |
| 8 | `m` e `c` na pista | 8 canais (C0..C7), calibração aceita | ___ |
| 9 | Gravar o `principal`, console `?` | tempo de leitura < 70 % do ciclo | ___ µs |
| 10 | Console `motores` (rodas no ar) | esq. frente, esq. ré, dir. frente, dir. ré | ___ |
| 11 | SW2 ligada: o ESC arma (bipes do ESC) | arma sozinho | ___ |
| 12 | Console `edf` (EDF presa, mão longe) | gira 3 s no nível 0 | ___ |
| 13 | Console `ir` com o controle | `NEC end 0 cmd 69 = PARTIDA`, `cmd 70 = PARADA` | ___ |

## Firmware no PC
Conferido em 17/11/2026, sem o robô (detalhes em [`firmware/README.md`](../firmware/README.md#testes-no-pc)):
- simulador (`firmware/testes/sim.cpp`) com `PERIODO_US` = 2000: "tudo certo";
- teste dos decodificadores IR (`firmware/testes/teste_ir.cpp`): "tudo certo";
- `principal` e `teste_barra` compilam sem erros nem avisos contra cabeçalhos que imitam o Arduino-ESP32 3.3 (a conferência real é na IDE).
