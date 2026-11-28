# Firmware

| Pasta | Conteúdo |
|---|---|
| [`principal/`](principal/) | firmware de competição (ESP32-C3 SuperMini). Abrir `principal.ino` |
| [`teste_barra/`](teste_barra/) | teste de bancada da barra de sensores (validado). Abrir `teste_barra.ino` |
| [`testes/`](testes/) | simulador do controle e teste dos decodificadores IR, no PC |

O `principal` é o firmware do perseguidor [RUSH - Dreamcast](https://github.com/therickmatos/Perseguidor-de-linha) (`firmware/perseguidor/`) passado para esta placa: o mesmo controle, o mesmo módulo de início por IR e o mesmo console. A leitura e a calibração da barra são o `barra.h/.cpp` do teste de bancada, copiado sem mudança. O Dreamcast é baseado no [EVA-RT 1.2.2](https://github.com/EXOTIC-TEAM-MX/EVA-RT) (© 2025 EXOTIC TEAM MX, AGPL-3.0); por isso este firmware também é AGPL-3.0.

**Estado (17/11/2026): compila contra cabeçalhos que imitam o Arduino-ESP32 3.3 e passa no simulador e no teste do IR; ainda não rodou no robô.** A compilação de verdade é a da IDE; os ganhos, o freio e a EDF só se acertam na pista.

## Gravar
1. Arduino IDE com o pacote **esp32 da Espressif, versão 3.x** (Gerenciador de Placas). Não precisa de biblioteca extra (`Preferences` vem no pacote).
2. Placa: **ESP32C3 Dev Module** (ou "Nologo ESP32C3 Super Mini").
3. Ferramentas → **USB CDC On Boot: Enabled** (sem isso o sketch não compila, de propósito). Com arduino-cli: `--fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc`.
4. **SW1 e SW2 desligadas** (motivo em `eletronica/README.md`), cabo USB-C na SuperMini, gravar.
5. Monitor serial em 115200. Para usar o console, deixe a SW1 desligada (a USB alimenta a lógica; com a bateria ligada, os motores e o ESC funcionam).

Se a gravação não começar: segurar BOOT, apertar e soltar RESET, soltar BOOT e gravar de novo.

`teste_barra/` grava do mesmo jeito. O `teste_barra.ino` daqui já usa os pinos da placa Galiléia3 (S0..S3 = GPIO0..3, SIG = GPIO4); no teste validado, em protoboard, eram GPIO5, 6, 7, 10 e GPIO3. Comandos dele: `m` canais, `c` calibrar, `d` diagnóstico, `t` varredura de tempo, `l` cor da linha, `p` CSV, `r` descartar calibração, `x` apagar NVS. Ele liga com linha **branca em fundo preto** (a da pista); se aparecer "Linha PRETA" no início é porque há uma cor antiga gravada: `l` troca e grava. A calibração que ele grava vale para o `principal` (mesma NVS, se o mapa for o mesmo); a cor não: o `principal` sempre liga com a do `config.h` (branca).

## Uso
Botão 1 = botão da placa (GPIO21). Botão 2 = **BOOT** da SuperMini (GPIO9). Não há LED livre (o LED azul da SuperMini está no GPIO8, que é o sinal do ESC): os avisos saem como **bipe nos motores** (as rodas alternam de sentido na frequência do som; o robô não anda). Volume: parâmetro `bipe` (0 = mudo).

| Situação | Controle IR | Botão 1 | Botão 2 |
|---|---|---|---|
| Esperando (ao ligar) | partida é recusada (bipe grave) | calibrar | usar a calibração gravada |
| Esperando, segurando | — | teste das rodas | teste da EDF (3 s no nível atual) |
| Calibrando | — | terminar | — |
| Pronto | **partida: sai na hora**; programar grava a arena | sai em 1 s, **só para teste** (segurar: calibrar de novo) | sobe um nível (segurar: nível 0) |
| Correndo | **parada: freia e volta sozinho para "pronto"** | desliga | desliga |
| Fim sem o comando de parada | parada → "pronto"; partida → sai de novo | volta para "pronto" | volta para "pronto" |

| Bipe | Significa |
|---|---|
| 1 curto agudo ao ligar | firmware rodando |
| + 3 graves | leitura da barra lenta para o ciclo (ver `?` no console) |
| 1 curto | começou a calibrar / subiu um nível / (segurando) já pode soltar |
| 2 curtos | calibração aceita / calibração gravada em uso |
| 1 longo grave | calibração rejeitada, não há calibração gravada, partida sem calibrar, ou nível 0 |
| 3 curtos graves | já está no nível máximo |
| 1 de 150 ms | entrou em "pronto" |
| 3 curtos agudos | arena programada pelo controle / EDF vai ligar (teste) |
| 2 longos graves | fim: perdeu a linha |
| 3 longos graves | fim: botão ou tempo máximo |

- **Calibração:** passar a barra inteira pela linha (como no `c` do teste, mas sem tempo: o botão 1 termina). É rejeitada se algum sensor tiver contraste menor que 150 (cru); a anterior continua valendo. Fica gravada na NVS.
- **Ao ligar**, a calibração gravada é carregada, mas só vale depois do botão 2 (como no Dreamcast: decisão consciente de usar a calibração antiga).
- **Nível:** cada toque soma `passo` no PWM das rodas e `vpasso` na EDF. Sem LED, conte os toques (ou segure para voltar ao 0); o console mostra o nível.
- **EDF:** fora da partida o firmware manda o pulso mínimo (1000 µs) o tempo todo, então o ESC arma sozinho quando a SW2 é ligada. Na largada a EDF sobe em 200 ms junto com as rodas.
- **Largada:** com a barra sobre a linha (se não vir a linha, nem sai).
- **Rounds:** depois do comando de parada o robô volta para "pronto" com a mesma calibração e o mesmo nível.

## Módulo de início (IR)
Igual ao do Dreamcast (`ir.h/.cpp` copiados sem mudança; detalhes no [README do Dreamcast](https://github.com/therickmatos/Perseguidor-de-linha/blob/main/firmware/README.md#módulo-de-início-ir)):
- **controle NEC do kit do KY-022**, ligado de fábrica: partida e parada nas teclas 0x45 e 0x46 ("1" e "2" no controle de 17 teclas, "CH−" e "CH" no de 21), endereço 0. Outro controle igual por perto também larga e para o robô: de preferência, usar um controle NEC de outro endereço e acertar `necend`, `necpart` e `necpara` com o comando `ir`;
- **RC5 do startmodule.com** ao mesmo tempo: programar = endereço 0x0B, partida/parada = endereço 0x07, comando = arena + 1 / arena.

## Conferir na bancada, nesta ordem
1. **Leitura:** console `?`. O tempo de leitura da barra tem de ficar abaixo de 70 % do `PERIODO_US` (2000 µs). Se não ficar, usar os valores da varredura `t` do `teste_barra` em `LEITURA_*` (`config.h`).
2. **Canais da barra:** console `m` (passe a mão na barra por 8 s): os canais ativos têm de ser os do `MAPA_PADRAO` do `barra.h`.
3. **Rodas** (no ar): console `motores` (ou segurar o botão 1 em "esperando"). A sequência é esquerda para a frente, esquerda para trás, direita para a frente, direita para trás (1 a 4 bipes antes de cada). Se não bater, mudar `INVERTER_MOTOR_*` / `TROCAR_MOTORES` no `config.h` ou inverter os fios.
4. **Lado da barra:** calibrar e usar o console `s`. Com a linha sob a ponta **esquerda** do robô, quem muda é a primeira coluna e o erro é **negativo**. Se estiver ao contrário, `BARRA_ESPELHADA 1`.
5. **IR:** console `ir` e os botões do controle. Com o controle do kit: `NEC end 0 cmd 69 = PARTIDA` e `cmd 70 = PARADA`.
6. **EDF:** SW2 ligada (o ESC arma com bipes), EDF presa, console `edf`. Começar com `vent` baixo.
7. **Parada:** em "pronto", com as rodas no ar, partida e parada pelo controle. Depois no chão, no nível 0, com a mão perto.

## Console pelo celular (Bluetooth)
Há um app com botões para isso em [`app/`](../app/README.md). Abaixo, o uso por um terminal comum.

O mesmo console da USB, pelo BLE do ESP32-C3 (não é Bluetooth clássico: não aparece na lista de pareamento do celular, só dentro do app).
1. App: **Android "Serial Bluetooth Terminal"** (Kai Morich) → menu → Devices → aba **Bluetooth LE** → "Perseguidor-qx". **iOS: "Bluefruit Connect"** → conectar → UART.
2. Mande `senha=1234` (troque `BLE_SENHA` no `config.h` antes da competição). Sem a senha, nenhum comando vale: qualquer celular por perto conseguiria mandar `motores` ou `edf`.
3. Daí em diante, os mesmos comandos da tabela abaixo. A senha vale até desconectar.

- Com o Bluetooth, dá para ajustar com a SW1 ligada e sem cabo: mudar, `salvar`, voltar para "pronto" e largar.
- Como na USB, o console só responde com o robô parado; o que chegar durante a partida fica para depois.
- Conferir no `res` depois das primeiras voltas se `atrasados` continua em 0: a pilha do BLE pode atrasar um ciclo de vez em quando. Se atrapalhar, `BLE_CONSOLE 0`.
- Se a IDE disser que o sketch não cabe ("Sketch too big"): Ferramentas → Partition Scheme → **Huge APP (3MB No OTA/1MB SPIFFS)**. A NVS fica no mesmo lugar (parâmetros e calibração continuam).

## Console (USB ou Bluetooth, só com o robô parado)
`?` estado · `p` lista · `nome=valor` muda · `salvar` grava na NVS · `padrao` valores de fábrica · `arena=n` número da arena · `cor` linha branca/preta até desligar · `ir` quadros IR · `s` sensores ao vivo · `r` canais crus e pino do IR · `m` confere os canais · `res` resumo da última partida · `log` últimos 2,4 s em CSV · `motores` teste das rodas · `edf` teste da EDF

| Parâmetro | Padrão | O que é |
|---|---|---|
| `kp`, `kd` | 0,0733 / 0,4885 | PD. Erro em milésimos de sensor (1000 = 9,525 mm na QRE-8D); `kd` por ms. São os 0,06 / 0,40 da barra V1 (1000 = 7,8 mm) no mesmo PWM por mm |
| `fd` | 50 | filtro da derivada (% da leitura nova por ciclo) |
| `vel` | 40 | PWM nas curvas fechadas, nível 0 |
| `reta` | 10 | PWM a mais nas retas (0 = velocidade única) |
| `passo`, `nivel` | 5 / 0 | aumento por nível, nível ao ligar |
| `vent`, `vpasso` | 100 / 10 | EDF no nível 0 (0..255 = 1000..2000 µs) e aumento por nível. Chute baixo: subir na pista |
| `ecurva` | 491 | erro que indica entrada de curva (4,7 mm): volta para `vel` na hora |
| `treta` | 60 | ms com o erro quieto para poder passar de `vel` |
| `rreta`, `rcurva` | 15 / 40 | curvatura (% do PWM) até a qual anda em `vel + reta` / a partir da qual anda em `vel` |
| `acel` | 300 | PWM por segundo ao acelerar (também é a rampa da largada) |
| `fcurva`, `tfcurva` | 100 / 40 | freada ao baixar a velocidade: % da queda aplicada a mais, por uns `tfcurva` ms |
| `re` | 255 | ré máxima da roda de dentro |
| `perdido` | 100 | ms sem ver a linha até desligar |
| `tmax` | 180 | s até desligar sozinho (0 = sem limite) |
| `freio`, `tfreio` | 100 / 75 | ré no comando de parada: % do PWM em que vinha, por `tfreio` ms |
| `nec`, `necend`, `necpart`, `necpara` | 1 / 0 / 69 / 70 | controle NEC: liga, endereço, tecla de partida, tecla de parada |
| `bipe` | 40 | volume do bipe nos motores (0 = mudo). Se o robô se mexer no bipe, diminuir |

## Ajuste na pista
1. `reta=0`, nível 0: acertar `kp`/`kd` até seguir sem balançar (balança rápido → menos `kp` ou mais `kd`; demora a entrar na curva → mais `kp`).
2. **EDF:** subir `vent` aos poucos até o robô não escorregar nas curvas; olhar o aquecimento do ESC e da bateria.
3. **Parada:** mandar parada numa reta. Se o robô voltar para trás, diminuir `tfreio`; se demorar a parar, aumentar.
4. Subir o nível (botão 2) até o limite das curvas fechadas. Depois subir `reta` aos poucos (`res` mostra a % do tempo em reta).
5. `salvar`.

## O que muda em relação ao Dreamcast
| Dreamcast (ATmega328P) | Perseguidor-qx (ESP32-C3) | Por quê |
|---|---|---|
| ADC de 8 bits, calibração no `linha.cpp` | `barra.h/.cpp` do teste de bancada (12 bits, calibração com média móvel, NVS); o controle recebe 0..255 | usar a leitura já validada |
| ciclo de 1 ms | 2 ms (`PERIODO_US`); os tempos do resumo contam ms; o limite de salto da linha e o filtro da curvatura acompanham o período | leitura dos 12 sensores com 4 amostras cabe com folga |
| 2× DRV8874 (PH/EN), 976 Hz | TB6612 com PWM nas entradas (anda/freia), 20 kHz | PWMA/PWMB fixos em 3V3 nesta placa |
| ventoinha por MOSFET (PWM) | EDF por ESC (pulso de 1000–2000 µs a 50 Hz), mínimo o tempo todo | o ESC arma sozinho ao ligar a SW2 |
| 3 LEDs | bipe nos motores | não há LED livre |
| botões nos pinos do ATmega; ligar com botão = modo teste | botão da placa + BOOT; teste das rodas e da EDF segurando os botões em "esperando" | o GPIO21 é o TX da UART0 e o BOOT no boot entra em gravação |
| compensação pela bateria (A7) | sem compensação (`escala` 256) | a placa não mede a bateria |
| EEPROM | NVS (`Preferences`) | — |
| console pela serial/Bluetooth | console pela USB nativa | — |

Igual: posição pelo grupo de sensores em cima da linha, linha de partida segura o erro, velocidade de reta e de curva com freada na entrada de curva, roda que satura tira das duas, linha perdida continua virando para o lado em que saiu, módulo de início NEC + RC5, parada com ré curta e volta para "pronto".

## Testes no PC
```bash
cd firmware/testes
```

```bash
g++ -O2 -std=c++11 -I../principal sim.cpp ../principal/linha.cpp ../principal/corrida.cpp -o sim && ./sim
```

```bash
g++ -O2 -std=c++11 -I../principal teste_ir.cpp ../principal/ir.cpp -o teste_ir && ./teste_ir
```

Os dois terminam com "tudo certo" ou listam o que falhou. Depois de mexer em `linha.cpp`, `corrida.cpp` ou `ir.cpp`, rodar os dois.

- **`sim.cpp`**: o simulador do Dreamcast, com a leitura passando pela mesma normalização do `barra.cpp` (12 bits) e pela conversão do `hw.cpp`. Pista de ensaio, circuito de perseguidor inventado dentro do regulamento e as pistas TRIF 2025/2024, com três modelos de motor. A barra é a QRE-8D (8 sensores em linha reta, a 9,525 mm; até 27/11 era a V1, em arco, com 12 a 7,8 mm). O resto da geometria (barra a 107,1 mm do eixo, 110 mm entre as rodas) ainda é a do Dreamcast: trocar `L_BARRA` e `BITOLA` quando o robô estiver montado. O modelo dos motores é inventado: serve para conferir a lógica, **não** para escolher ganhos.
- Com `PERIODO_US` = 2000 ou 1000: tudo certo. Com 3000 o robô perde a linha no nível 12: não usar mais de 2000.
- Com a QRE-8D a linha de 19 mm cobre dois sensores inteiros e o robô balança uns 3,5 mm na reta (zona morta). Por isso o teste da linha de partida compara o desvio com e sem ela na mesma reta (até 2 mm a mais), e não com 3 mm fixos.
- O nível 12 (PWM 100/110) com curvas de R100 fica no limite com a QRE-8D: mudar `kp` ou `kd` em ±3 % já faz esse caso perder a linha ou contar errado as linhas de partida. Os níveis 0 a 8 passam. A barra V1, mais larga (86 mm entre os sensores das pontas, contra 67 mm), passava com folga nos mesmos ±3 %. Na pista, subir de nível aos poucos.
- **`teste_ir.cpp`**: o do Dreamcast, sem mudança.

## Pendências
- Tudo acima precisa ser confirmado no robô: compilação na IDE, tempo de leitura, mapa do mux, sentido das rodas, lado da barra, IR, EDF, ganhos, freio.
- Volume do bipe: 40 é chute. Se não der para ouvir, aumentar; se o robô se mexer, diminuir.
- Geometria do simulador (`L_BARRA`, `BITOLA`) e modelo dos motores.
- O LED azul da SuperMini fica aceso quase o tempo todo (é o sinal do ESC).
