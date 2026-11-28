# Eletrônica — placa Galiléia3

Placa base do Perseguidor-qx, desenhada no EasyEDA Pro (projeto `Galiléia3`). Fenolite de 1,5 mm, componentes PTH e módulos prontos.

| Pasta | Conteúdo |
|---|---|
| [`easyeda/`](easyeda/README.md) | projeto `.epro` (fonte editável). **Ainda vazia:** o `.epro` entra depois de revisado no EasyEDA |
| [`fabricacao/`](fabricacao/README.md) | Gerber `.zip` para fabricar ou corroer. **Ainda vazia** |
| [`ferramentas/`](ferramentas/) | scripts que leem o `.epro`: netlist, conferência esquemático × PCB e a troca de netports por rótulos |
| `esquematico.pdf` | **falta**: exportar do EasyEDA depois de abrir o `.epro` corrigido |

## Módulos e componentes
| Ref. | Peça | Função |
|---|---|---|
| U1 | ESP32-C3 SuperMini | microcontrolador (USB-C nativa para gravar e para o console) |
| U2 | módulo TB6612FNG | driver dos dois motores |
| U3 | Mini360 (MP2307) | 3,3 V a partir da bateria, depois da SW1 |
| U4 | XT60PB-M | bateria |
| U5 | barra de pinos 1×8 | cabo do módulo CD74HC4067 com a QRE-8D (ver "Barra de sensores"); era a barra V1 |
| U6 | barra de pinos 1×3 | receptor IR KY-022 (módulo de início) |
| U7 | barra de pinos 1×3 | sinal do ESC da EDF |
| U8 | botão 6×6 (TSWDIP) | botão 1 do firmware |
| SW1 | SS12D00G3 | liga a lógica: VBAT → VSW (entrada do Mini360) |
| SW2 | SS12D00G3 | liga o sinal do ESC: GPIO8 → ESC_SIG |
| R3 | 10 kΩ 1206 | pull-up do GPIO2 (pino de strapping, usado como S2 do mux) |
| R4 | 10 kΩ 1206 | pull-down do ESC_SIG (ESC sem sinal flutuando) |
| C2 | 100 nF | desacoplamento do 3V3 |
| C3 | 470 µF / 25 V (10 × 12,5) | reservatório do VBAT, perto do VM do TB6612 |

## Mapa de pinos do ESP32-C3 SuperMini
Conferido na netlist do `.epro` (`ferramentas/netlist_epro.py`). É o mesmo do `firmware/principal/config.h`.

| GPIO | Rede | Vai para | Observação |
|---|---|---|---|
| 0 | S0 | barra, pino 8 | seleção do mux |
| 1 | S1 | barra, pino 7 | seleção do mux |
| 2 | S2 | barra, pino 6 | strapping; pull-up de 10 kΩ (R3) |
| 3 | S3 | barra, pino 5 | seleção do mux |
| 4 | SIG | barra, pino 4 | saída do mux, ADC1_CH4 |
| 5 | BIN1 | TB6612 BI1 | motor direito |
| 6 | START | KY-022, sinal | módulo de início (IR) |
| 7 | BIN2 | TB6612 BI2 | motor direito |
| 8 | ESC_SW | SW2 → ESC_SIG → ESC | strapping e LED azul da SuperMini; R4 puxa o ESC_SIG para o GND |
| 9 | — | — | botão BOOT da SuperMini (botão 2 do firmware) |
| 10 | AIN1 | TB6612 AI1 | motor esquerdo |
| 20 | AIN2 | TB6612 AI2 | motor esquerdo (é o RX da UART0, livre com o console na USB) |
| 21 | BTN | U8 (botão) → GND | botão 1 do firmware (é o TX da UART0: não segurar o botão ao ligar) |
| 3V3 | 3V3 | Mini360 OUT+ | alimentação da lógica (ver "Mini360") |
| 5V | NC_5V | — | sem ligação |

TB6612: PWMA, PWMB, STBY e VCC no 3V3; VM no VBAT. Por isso o PWM vai nas entradas IN1/IN2 (modo anda/freia, ver `firmware/README.md`).
Saídas: AO1/AO2 = `M_ESQ_A`/`M_ESQ_B` (motor esquerdo), BO1/BO2 = `M_DIR_A`/`M_DIR_B` (motor direito).

## Redes
| Rede | Pinos |
|---|---|
| VBAT | XT60 pino 1, SW1 comum, C3 +, TB6612 VM |
| VSW | SW1 pino 1, Mini360 IN+ |
| 3V3 | Mini360 OUT+, ESP 3V3, TB6612 VCC/STBY/PWMA/PWMB, R3, C2, barra pino 1, KY-022 pino 2 |
| GND | XT60 pino 2, Mini360 IN−/OUT−, ESP GND, TB6612 GND (pinos 3 e 9), C2, C3, R4, barra pino 2, KY-022 pino 3, ESC pino 3, botão |
| ESC_SW | ESP GPIO8, SW2 comum |
| ESC_SIG | SW2 pino 1, R4, ESC pino 1 |
| START | ESP GPIO6, KY-022 pino 1 |
| BTN | ESP GPIO21, botão pino 1 |
| S0..S3, SIG | ESP GPIO0..4, barra pinos 8..4 |
| AIN1, AIN2, BIN1, BIN2 | ESP GPIO10, 20, 5, 7 → TB6612 |
| M_ESQ_A/B, M_DIR_A/B | saídas do TB6612 (os fios dos motores) |
| sem ligação | ESP 5V (`NC_5V`), BEC do ESC (`NC_ESC_BEC`), SW1 pino 3, SW2 pino 3, pinos 2 e 3 do botão, barra pino 3 (`P6`), ESP GPIO9, TB6612 pino 8 (GND) |

Conector da barra (U5): 1 = 3V3, 2 = GND, 3 = `P6` (sem ligação), 4 = SIG, 5 = S3, 6 = S2, 7 = S1, 8 = S0.

## Barra de sensores: QRE-8D + módulo CD74HC4067 (desde 28/11/2026)
A barra V1 (12× QRE1113 + 74HC4067 na própria placa) não pôde ser montada. No lugar dela vai a **QRE-8D da RoboCore** (8× QRE1113GR a 9,525 mm, 75 × 13 mm; esquemático `QRE-8D_v1.0_schematic.pdf` da RoboCore, desenho original da Pololu) lida por um **módulo CD74HC4067** ligado no mesmo conector U5. A placa Galiléia3 não muda.

A QRE-8D é de "saída digital": cada saída é um capacitor de 10 nF (para o VCC) que o fototransistor descarrega, com 220 Ω até o pino, e cada uma precisaria de um GPIO só para ela. A SuperMini não tem pinos para isso (8 sensores + 4 do TB6612 + ESC + IR = 14 de 13). Com um **pull-up em cada saída** ela vira analógica (como uma QTR-8A): a tensão cai quando o sensor vê o branco. O mux passa as 8 para o ADC, como fazia a barra V1, e o `barra.cpp` lê igual.

| U5 (placa) | Módulo CD74HC4067 | QRE-8D (J4, 11 pinos) |
|---|---|---|
| 1 — 3V3 | VCC | 2 — VCC |
| 2 — GND | GND e **EN** (habilita o mux) | 1 — GND |
| 3 — sem ligação | — | 3 — LEDON: **deixar solto** (R15 de 47 k liga os LEDs) |
| 4 — SIG (GPIO4) | SIG | — |
| 5 — S3 (GPIO3) | S3 | — |
| 6 — S2 (GPIO2) | S2 | — |
| 7 — S1 (GPIO1) | S1 | — |
| 8 — S0 (GPIO0) | S0 | — |
| — | C0 … C7 | 4 … 11 — saídas 1 … 8 (saída 1 no C0, saída 8 no C7) |

- **Pull-ups:** um resistor de cada saída (pinos 4 a 11 do J4, ou C0..C7 do módulo) para o VCC. Começar com **22 kΩ** (filtro de 0,22 ms com o capacitor de 10 nF). Na bancada, o branco a 3 mm tem de ficar bem abaixo do preto: se o delta da calibração (`c`) ficar pequeno, subir para 47 kΩ (0,47 ms). Uma rede SIP de 8 resistores com comum serve.
- **J2 "3V3 Bypass" fechado** (ponte de solda ou jumper): com VCC = 3,3 V, ele tira os 23,5 Ω (R13 ∥ R14) do caminho dos LEDs e a corrente de cada par sobe de ~6 para ~12 mA (estimativa). **Nunca fechar com VCC de 5 V.**
- O módulo e a QRE-8D no **3V3** (pino 1 do U5), nunca em 5 V: o SIG não pode passar de 3,3 V.
- **Preto saturado é normal:** sem reflexo, a saída fica perto de 3,3 V, acima da faixa do ADC em 11 dB (~2,5 V). O `teste_barra` mostra `[SATURADO]` no preto; o que importa é o branco bem abaixo.
- Não separar a placa no "PCB Break Point" (as saídas 7 e 8 perdem o controle dos LEDs; o J1 é para esse caso).
- Mapa no firmware: `MAPA_PADRAO {0..7}` e `NUM_SENSORS 8` (`barra.h`), `N_SENSORES 8` (`config.h`). Se a saída 1 ficar à direita do robô, `BARRA_ESPELHADA 1`.

## Chaves e cuidados
- **SW1** liga só a lógica (Mini360 → 3V3). O VM do TB6612 e o C3 ficam no VBAT, antes da chave: com a bateria ligada eles estão sempre energizados. Com a SW1 desligada o 3V3 cai, as entradas do TB6612 vão a 0 (pull-down interno) e os motores param. Para guardar o robô, tirar a bateria.
- **SW2** liga o sinal do ESC. Desligada, o R4 segura o ESC_SIG em 0 e a EDF não arma. O ESC é alimentado direto pela bateria (só sinal e GND passam pela placa; o BEC fica sem ligação).
- **Gravar e usar o console pela USB: SW1 e SW2 desligadas.**
  - SW1: com ela ligada, o 3V3 viria do Mini360 e do regulador da SuperMini (pela USB) ao mesmo tempo. Desligada, a USB alimenta a lógica sozinha; com a bateria conectada os motores e o ESC continuam funcionando, o que serve para testar na bancada pelo console.
  - SW2: o GPIO8 é pino de strapping e o modo de gravação precisa dele em 1. Com a SW2 ligada, o R4 (10 kΩ) puxa o GPIO8 para 0. Depois de gravar, a SW2 pode ser ligada para testar a EDF pelo console (`edf`).
- Não segurar o botão da placa (GPIO21) ao ligar: no boot a ROM escreve no TX da UART0, que é esse pino.

## Mini360
O MP2307 aceita 4,75–23 V na entrada e regula de 1 a 17 V na saída. **A saída tem de estar em 3,3 V antes de encostar no 3V3 da placa:** o módulo costuma vir regulado em outro valor, e mais de 3,6 V queima a SuperMini, a barra e o KY-022.
1. Antes de soldar o Mini360 (ou com o ESP, a barra e o KY-022 fora da placa), ligar a bateria na entrada (IN+/IN−).
2. Girar o trimpot devagar, medindo, até a saída marcar **3,30 V** no multímetro.
3. Soldar, ligar a SW1 sem a SuperMini e medir de novo no pino 3V3 antes de colocar os módulos.
4. Anotar a tensão em [`docs/testes.md`](../docs/testes.md).

## Correções no esquemático (17/11/2026)
Feitas por `ferramentas/netports_para_rotulos.py` no `.epro` de 17/11 (`ProPrj_Galil_ia3_2026-11-17.epro`), entregue fora do repositório como `ProPrj_Galileia3_2026-11-17_rotulos.epro`:
- os 66 netports (as setas grandes) viraram fios curtos com o rótulo de rede em cima, como o da bateria. Cada lado de componente tem o fio do comprimento do maior nome dele;
- o rótulo do pino 1 do XT60 era `BAT`, e o resto do circuito usa `VBAT`: no esquemático, a bateria não ligava em nada. No PCB o pad já estava em `VBAT`. Agora o esquemático também diz `VBAT`;
- nenhuma outra ligação mudou. A netlist nova bate com os 70 pads do PCB (`ferramentas/confere_pcb.py`).

Fica para o Erick decidir:
- o pino 8 (GND) do módulo TB6612 está sem ligação no esquemático e no PCB. O módulo liga os GND por dentro, então funciona; ligar ao GND só reforça o retorno da corrente dos motores;
- trocar os rótulos `NC_*` por marcadores de "sem ligação" do EasyEDA (hoje cada pino livre tem uma rede própria com um pino só).

## Ferramentas
Os scripts leem o `.epro` extraído (é um zip): `unzip -d pasta projeto.epro`.

```bash
python3 -I eletronica/ferramentas/netlist_epro.py pasta pasta/SHEET/*/1.esch
```

```bash
python3 -I eletronica/ferramentas/confere_pcb.py pasta
```

```bash
python3 -I eletronica/ferramentas/netports_para_rotulos.py pasta
```

O último altera o `.esch` dentro da pasta; depois é só zipar de novo com a mesma estrutura.
