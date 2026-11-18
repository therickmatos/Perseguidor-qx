# Perseguidor-qx

Robô perseguidor de linha com sucção por EDF (QX-Motor 30 mm), placa **Galiléia3** em fenolite, para o Perseguidor de Linha do TRIF 2026 (regras da RoboCore).

> Foto do robô: pendente, depois da montagem (vai para [`docs/imagens/`](docs/imagens/)).

- **ESP32-C3 SuperMini**, firmware próprio (`firmware/principal/`), console pela USB
- **TB6612FNG** para os dois motores; rodas de Ø22 mm
- **Barra de sensores V1**: 12× QRE1113 + 74HC4067, presa em braços impressos à frente da placa
- **EDF QX-Motor 30 mm** por ESC, presa num anel impresso com a entrada a ~2 mm do chão; chave SW2 no sinal do ESC
- **Módulo de início** por IR: KY-022 (controle NEC do kit e padrão RC5 do startmodule.com)
- **Mini360** para o 3,3 V; bateria no **XT60**; chave SW1 na lógica

## Onde encontrar
| Para… | Abra |
|---|---|
| ver o mapa de pinos, as redes e ajustar o Mini360 | [`eletronica/README.md`](eletronica/README.md) |
| o projeto do EasyEDA e o Gerber | [`eletronica/easyeda/`](eletronica/easyeda/README.md) e [`eletronica/fabricacao/`](eletronica/fabricacao/README.md) (ainda vazios) |
| imprimir as peças (anel da EDF, braços da barra) | [`mecanica/stl/`](mecanica/stl/) e [`mecanica/README.md`](mecanica/README.md#o-que-imprimir) |
| parafusos e ordem de montagem | [`mecanica/README.md`](mecanica/README.md#ferragens) |
| gravar o firmware (SW1 e SW2 desligadas) | [`firmware/README.md`](firmware/README.md#gravar) |
| usar o robô: botões, bipes, controle IR | [`firmware/README.md`, "Uso"](firmware/README.md#uso) |
| testar a barra na bancada | [`firmware/teste_barra/`](firmware/teste_barra/) |
| comprar os componentes | [`docs/BOM.md`](docs/BOM.md) |
| anotar medidas e testes | [`docs/testes.md`](docs/testes.md) |

## Estado (17/11/2026)
- [x] Esquemático no EasyEDA Pro, corrigido em 17/11 (rótulos de rede no lugar dos netports; `BAT` → `VBAT` no XT60). O `.epro` entra no repositório depois de revisado
- [ ] Gerber e `esquematico.pdf`
- [x] Anel da EDF e braços da barra: scripts e STL (`mecanica/`). O robô não usa patins
- [x] Teste da barra de sensores, validado na bancada (`firmware/teste_barra/`)
- [x] Firmware de competição (`firmware/principal/`): passa no simulador e no teste do IR; **ainda não rodou no robô**
- [ ] Primeira energização ([`docs/testes.md`](docs/testes.md))
- [ ] Montagem e ajuste na pista

## Pendente
Depende de acesso ao EasyEDA ou da montagem:
- [ ] `.epro` revisado → [`eletronica/easyeda/`](eletronica/easyeda/README.md)
- [ ] Gerber `.zip` → [`eletronica/fabricacao/`](eletronica/fabricacao/README.md)
- [ ] `esquematico.pdf` → [`eletronica/`](eletronica/README.md)
- [ ] lista de componentes definitiva, com motores, bateria e ESC → [`docs/BOM.md`](docs/BOM.md)
- [ ] fotos do robô montado → [`docs/imagens/`](docs/imagens/)

## Estrutura
| Pasta | Conteúdo |
|---|---|
| [`eletronica/`](eletronica/README.md) | `easyeda/` (fonte editável), `fabricacao/` (Gerber), `ferramentas/` (scripts que leem o `.epro`) e o README com pinos, redes e Mini360 |
| [`mecanica/`](mecanica/README.md) | `stl/` (peças para imprimir e DXF dos furos da placa), `scripts/` (geradores em Python) e o README com ferragens, montagem e impressão |
| [`firmware/`](firmware/README.md) | `principal/` (competição), `teste_barra/` (bancada), `testes/` (simulador e teste do IR no PC) |
| [`docs/`](docs/) | `BOM.md`, `testes.md` e `imagens/` |

Projetos irmãos: o perseguidor [RUSH - Dreamcast](https://github.com/therickmatos/Perseguidor-de-linha), de onde vem o controle do firmware, e o seguidor [RUSH - SATURN](https://github.com/therickmatos/Seguidor-de-linha), de onde vem o teste da barra.

## Licença
GNU Affero General Public License v3.0 ([`LICENSE`](LICENSE)). O firmware é derivado do [EVA-RT](https://github.com/EXOTIC-TEAM-MX/EVA-RT) (© EXOTIC TEAM MX, AGPL-3.0), pelo firmware do Dreamcast, e mantém a mesma licença.
