# Testes

Registro do que foi medido no robô. Preencher as lacunas (`___`) e datar.

## Barra de sensores (bancada)
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

- **24/11/2026, no robô** (barra nos braços, `teste_barra`, 4 amostras, t = 2055 µs): os 12 sensores respondem. Cru sobre papel branco ≈ 1640–1690, sobre a fita preta ≈ 230–250; ruído ±10. **A leitura sobe com a reflexão** (branco alto), ao contrário do que o `barra.cpp` supunha: a inversão de cor foi corrigida (linha preta inverte). Nos dois casos os sensores aparecem em blocos de 4 (S0–S3, S4–S7, S8–S11): conferir com o `m` e a largura da fita.

### Barra V1 pelo Gerber (`therickmatos/Seguidor-de-linha`, `hardware/barra_v1/`), lido em 24/11/2026
- **J2 (1×8, passo 2,0):** 1 = VCC, 2 = GND, 3 = sem ligação, 4 = SIG, 5 = S3, 6 = S2, 7 = S1, 8 = S0. Mesma ordem do U5 da Galiléia3: o cabo é direto.
- **QRE1113:** coletor no VCC, emissor no SIG do canal com 4k7 para o GND (R0..R11). Por isso a leitura sobe no branco. LEDs dois a dois em série com 47 Ω (R20..R25): ~15–19 mA por LED a 3,3 V, ~110 mA no total.
- **74HC4067 em TSSOP-24 (passo 0,65 mm):** sensores S0..S11 da esquerda para a direita nos canais **2 a 13**; canais 0, 1, 14 e 15 e o E no GND. O firmware usa 4 a 15 (`MAPA_PADRAO`): conferir com o `m` depois de consertar o S0/S1.
- **Pontos apertados:** pino 10 (S0) entre o 9 (GND) e o 11 (S1); pino 11 (S1) entre o 10 e o 12 (GND), com 0,19 mm de cobre a cobre. Perto da J2, a trilha do S2 passa a 0,17 mm do pad do S1.
- **24/11:** primeiro o S0 sem efeito; depois do conserto, o S1 cai a 0,5 V com a barra ligada (3,3 V sem ela): curto na barra, provável ponte de solda nos pinos 10–12 do mux.

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
| 8 | `m` e `c` na pista | 12 canais, calibração aceita | ___ |
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
