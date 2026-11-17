# Lista de componentes (BOM)

> **Provisória.** Montada a partir do esquemático do `.epro` de 17/11/2026 e dos scripts de `mecanica/`. A lista de componentes que o Erick passou em outra conversa não estava disponível aqui: substituir ou completar com ela (quantidades de reserva, fornecedores, preços).

## Placa Galiléia3
| Ref. | Qtde | Componente | Observação |
|---|---|---|---|
| — | 1 | placa de fenolite 1,5 mm | furo Ø38 da EDF e 3× Ø4 do anel (`mecanica/stl/anel_turbina_placa.dxf`) |
| U1 | 1 | ESP32-C3 SuperMini | com barras de pinos |
| U2 | 1 | módulo TB6612FNG (16 pinos) | |
| U3 | 1 | módulo Mini360 (MP2307) | ajustar para 3,30 V antes de soldar (`eletronica/README.md`) |
| U4 | 1 | conector XT60PB-M (macho, para placa) | + XT60 fêmea na bateria |
| U5 | 1 | barra de pinos 1×8, 2,54 mm (fêmea no `.epro`) | cabo da barra de sensores |
| U6 | 1 | barra de pinos 1×3, 2,54 mm | KY-022 |
| U7 | 1 | barra de pinos 1×3, 2,54 mm | sinal do ESC |
| U8 | 1 | botão táctil 6×6×6 mm, 4 terminais | |
| SW1, SW2 | 2 | chave deslizante SS12D00G3 | |
| R3, R4 | 2 | resistor 10 kΩ 1206 | |
| C2 | 1 | capacitor 100 nF | |
| C3 | 1 | eletrolítico 470 µF / 25 V, 10 × 12,5 mm | |

## Módulos e peças do robô
| Qtde | Item | Observação |
|---|---|---|
| 1 | barra de sensores V1 (12× QRE1113 + 74HC4067) | a mesma do Saturn/Dreamcast; Gerber em `therickmatos/Seguidor-de-linha`, `hardware/barra_v1/` |
| 1 | cabo 8 vias para a barra | a barra tem passo de 2,0 mm e a placa 2,54 mm (cabo misto) |
| 1 | receptor IR KY-022 + controle NEC do kit | módulo de início |
| 1 | EDF QX-Motor 30 mm | corpo do duto Ø34,0, boca Ø37,5 |
| 1 | ESC para a EDF | sinal e GND pela placa; potência direto da bateria; BEC sem ligação |
| 2 | motores com redução | modelo a definir nesta lista |
| 2 | rodas Ø22 mm | a altura da placa nos scripts parte desse diâmetro |
| 1 | bateria LiPo com XT60 | tensão a definir nesta lista: o TB6612 aceita até 13,5 V no VM e o Mini360 até 23 V |

## Peças impressas e ferragens
Ver [`mecanica/README.md`](../mecanica/README.md#o-que-imprimir) (anel da EDF, braços da barra; patins ainda faltam) e [ferragens](../mecanica/README.md#ferragens): 3× M3×8 + 3 parlock M3, 1× M3×12 + porca M3, 2× M2,5×8 + porca, 2× M2 + porca.
