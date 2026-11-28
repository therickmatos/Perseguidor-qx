/*
 * console.cpp - comandos pela USB (115200) e pelo Bluetooth (ver canal.h)
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * https://github.com/EXOTIC-TEAM-MX/EVA-RT
 * Adaptado do console do perseguidor RUSH - Dreamcast.
 *
 * So atende com o robo parado (regra 3.1.2 do Perseguidor: ajuste entre
 * partidas). Durante a partida nada e lido nem escrito na serial.
 *
 *   ?            ajuda e estado
 *   p            lista os parametros
 *   nome=valor   muda um parametro (vale ate desligar)
 *   salvar       grava os parametros na NVS
 *   padrao       volta aos valores de fabrica (nao grava)
 *   arena=n      grava o numero da arena do modulo de inicio (par, 0..126)
 *   cor          troca linha branca/preta ate desligar (o padrao e o do config.h)
 *   ir           mostra os quadros RC5 e NEC recebidos (qualquer tecla para)
 *   s            sensores normalizados e erro (qualquer tecla para)
 *   r            leitura crua dos 16 canais do mux e pino do IR
 *   m            confere os canais do mux (8 s, passe a mao na barra)
 *   res          resumo da ultima partida
 *   log          ultimos segundos da ultima partida, em CSV
 *   motores      teste das rodas (esq. frente/re, dir. frente/re)
 *   edf          liga a EDF no nivel atual por 3 s
 */
#include <Arduino.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "canal.h"
#include "estado.h"
#include "hw.h"
#include "memoria.h"

// ---------------------------------------------------------------- tabela

enum { T_FLOAT, T_U8, T_U16 };

struct Descritor {
  const char *nome;
  uint8_t  tipo;
  uint8_t  desloc;
  uint16_t minimo;
  uint16_t maximo;
};

#define PARAM(campo, tipo, mn, mx) \
  { #campo, tipo, (uint8_t)offsetof(Parametros, campo), mn, mx }

static const Descritor TABELA[] = {
  PARAM(kp, T_FLOAT, 0, 2),
  PARAM(kd, T_FLOAT, 0, 20),
  PARAM(fd, T_U8, 1, 100),
  PARAM(vel, T_U8, 0, 255),
  PARAM(reta, T_U8, 0, 200),
  PARAM(passo, T_U8, 0, 50),
  PARAM(vent, T_U8, 0, 255),
  PARAM(vpasso, T_U8, 0, 50),
  PARAM(nivel, T_U8, 0, 40),
  PARAM(ecurva, T_U16, 50, 5500),
  PARAM(treta, T_U16, 0, 5000),
  PARAM(rreta, T_U8, 0, 200),
  PARAM(rcurva, T_U8, 0, 200),
  PARAM(acel, T_U16, 10, 20000),
  PARAM(fcurva, T_U8, 0, 200),
  PARAM(tfcurva, T_U16, 1, 500),
  PARAM(re, T_U8, 0, 255),
  PARAM(perdido, T_U16, 1, 2000),
  PARAM(tmax, T_U16, 0, 600),
  PARAM(freio, T_U16, 0, 300),
  PARAM(tfreio, T_U16, 0, 1000),
  PARAM(nec, T_U8, 0, 1),
  PARAM(necend, T_U16, 0, 65535),
  PARAM(necpart, T_U8, 0, 255),
  PARAM(necpara, T_U8, 0, 255),
  PARAM(bipe, T_U8, 0, 120),
};
#define N_PARAMETROS  (sizeof(TABELA) / sizeof(TABELA[0]))

static void imprimirParametro(const Descritor &d) {
  const uint8_t *campo = (const uint8_t *)&P + d.desloc;
  Con.print(d.nome);
  Con.print('=');
  if (d.tipo == T_FLOAT) {
    float f;
    memcpy(&f, campo, sizeof(f));
    Con.print(f, 3);
  } else if (d.tipo == T_U8) {
    Con.print(*campo);
  } else {
    uint16_t v;
    memcpy(&v, campo, sizeof(v));
    Con.print(v);
  }
}

static void listarParametros() {
  for (uint8_t i = 0; i < N_PARAMETROS; i++) {
    imprimirParametro(TABELA[i]);
    Con.print((i % 5 == 4 || i == N_PARAMETROS - 1) ? '\n' : '\t');
  }
}

static void definir(char *nome, const char *valor) {
  if (strcmp(nome, "arena") == 0) {
    long v = atol(valor);
    if (v < 0 || v > 126 || (v & 1)) {
      Con.println(F("arena: numero par de 0 a 126"));
      return;
    }
    arena = (uint8_t)v;
    arenaGravada = true;
    if (!memSalvarArena(arena)) Con.println(F("ERRO ao gravar a arena na NVS"));
    consoleArena();
    return;
  }
  for (uint8_t i = 0; i < N_PARAMETROS; i++) {
    const Descritor &d = TABELA[i];
    if (strcmp(nome, d.nome) != 0) continue;

    uint8_t *campo = (uint8_t *)&P + d.desloc;
    if (d.tipo == T_FLOAT) {
      float f = (float)atof(valor);
      if (!(f >= (float)d.minimo && f <= (float)d.maximo)) break;
      memcpy(campo, &f, sizeof(f));
    } else {
      long v = atol(valor);
      if (v < (long)d.minimo || v > (long)d.maximo) break;
      if (d.tipo == T_U8) {
        *campo = (uint8_t)v;
      } else {
        uint16_t w = (uint16_t)v;
        memcpy(campo, &w, sizeof(w));
      }
    }
    if (strcmp(nome, "nivel") == 0) nivelAtual = P.nivel;
    if (nivelAtual > nivelMaximo(P)) nivelAtual = nivelMaximo(P);
    imprimirParametro(d);
    Con.println();
    return;
  }
  Con.println(F("nome desconhecido ou valor fora da faixa ('p' lista)"));
}

// ---------------------------------------------------------------- relatorios

void consoleNivel() {
  Con.printf("nivel %u: curva %u reta %u EDF %u (%u us)\n", (unsigned)nivelAtual,
                (unsigned)velCurvaDoNivel(P, nivelAtual), (unsigned)velRetaDoNivel(P, nivelAtual),
                (unsigned)ventDoNivel(P, nivelAtual),
                (unsigned)(ESC_MIN_US + (uint32_t)ventDoNivel(P, nivelAtual) * (ESC_MAX_US - ESC_MIN_US) / 255));
}

void consoleArena() {
  Con.printf("modulo de inicio: arena %u%s -> partida = end %u cmd %u, parada = end %u cmd %u\n",
                (unsigned)arena, arenaGravada ? "" : " (padrao, nunca programada)",
                (unsigned)IR_END_PARTIDA, (unsigned)(arena | 1), (unsigned)IR_END_PARTIDA, (unsigned)arena);
  if (P.nec) {
    Con.printf("controle NEC ligado: end %u, partida cmd %u, parada cmd %u\n",
                  (unsigned)P.necend, (unsigned)P.necpart, (unsigned)P.necpara);
  }
}

// Tempo da leitura da barra, que tem de caber no ciclo de controle
void consoleTempoLeitura() {
  uint8_t norm[N_SENSORES];
  uint32_t soma = 0, maior = 0;
  for (uint8_t k = 0; k < 50; k++) {
    hwLerBarra(norm);
    uint32_t t = barra.tempoLeituraUs();
    soma += t;
    if (t > maior) maior = t;
  }
  Con.printf("leitura da barra: %lu us (max %lu) com %u amostras, %u us, descarte %s; ciclo de %u us\n",
                (unsigned long)(soma / 50), (unsigned long)maior, (unsigned)barra.amostras(),
                (unsigned)barra.acomodacaoUs(), barra.descarte() ? "sim" : "nao", (unsigned)PERIODO_US);
  if (maior * 10 > (uint32_t)PERIODO_US * 7) {
    Con.println(F("** LEITURA LENTA para o ciclo: reduza LEITURA_AMOSTRAS (varredura 't' do teste_barra) "
                     "ou aumente PERIODO_US no config.h **"));
  }
}

void consoleEstado() {
  Con.println(F(FW_NOME " " FW_VERSAO " - baseado no EVA-RT (c) EXOTIC TEAM MX, AGPL-3.0"));
  Con.printf("calibracao: %s | linha %s\n",
                temCal ? "ok" : (barra.calibrado() ? "gravada (botao 2 para usar)" : "falta"),
                barra.corLinha() == LINHA_BRANCA ? "BRANCA em fundo preto" : "PRETA em fundo branco");
  consoleNivel();
  consoleArena();
  consoleTempoLeitura();
#if BLE_CONSOLE
  Con.printf("bluetooth: \"%s\", %s\n", BLE_NOME,
             Con.bleConectado() ? (Con.bleLiberado() ? "conectado" : "conectado, sem senha") : "esperando conexao");
#endif
}

static void ajuda() {
  consoleEstado();
  Con.println(F("comandos: ?  p  nome=valor  salvar  padrao  arena=n  cor  ir  s  r  m  res  log  motores  edf"));
}

void consoleCalibracao(const ResultadoCalibracao &r) {
  Con.println(F("sensor\tcanal\tmin\tmax\tdelta"));
  for (uint8_t i = 0; i < N_SENSORES; i++) {
    int32_t d = (int32_t)r.vMax[i] - (int32_t)r.vMin[i];
    Con.printf("S%u\tC%u\t%u\t%u\t%ld%s%s\n", (unsigned)i, (unsigned)barra.canalDoSensor(i),
                  (unsigned)r.vMin[i], (unsigned)r.vMax[i], (long)d,
                  d < DELTA_MINIMO ? "\t<-- SEM CONTRASTE" : "",
                  r.vMax[i] >= LIMIAR_SATURACAO ? "\t[SATURADO]" : "");
  }
}

void consoleResumo() {
  const Resumo &r = corrida.resumo();
  Con.print(F("fim: "));
  switch (r.motivo) {
    case FIM_PARADA:  Con.print(F("COMANDO DE PARADA")); break;
    case FIM_PERDEU:  Con.print(F("PERDEU A LINHA")); break;
    case FIM_BOTAO:   Con.print(F("BOTAO")); break;
    case FIM_TEMPO:   Con.print(F("TEMPO MAXIMO")); break;
    default:          Con.println(F("nenhuma partida")); return;
  }
  Con.printf(" | andou %.3f s | linhas de partida %u (~%u voltas)\n", r.totalMs / 1000.0f,
                (unsigned)r.faixas, (unsigned)(r.faixas / 2));
  Con.printf("curva %u reta %u EDF %u | em reta %lu %% do tempo\n", (unsigned)r.velCurva,
                (unsigned)r.velReta, (unsigned)r.vent,
                (unsigned long)(r.totalMs ? (r.retaMs * 100UL) / r.totalMs : 0));
  Con.printf("erro max %u | sem linha %u ms\n", (unsigned)r.erroMax, (unsigned)r.perdidoMs);
  Con.printf("ciclo max %lu us (periodo %u), atrasados %lu\n", (unsigned long)cicloMaxUs,
                (unsigned)PERIODO_US, (unsigned long)ciclosAtrasados);
}

static void imprimirLog() {
  Con.println(F("ms,erro,esq,dir,vista,faixa,reta"));
  uint8_t n = corrida.logQtd();
  for (uint8_t i = 0; i < n; i++) {
    const AmostraLog &a = corrida.logAmostra(i);
    Con.printf("%ld,%d,%d,%d,%u,%u,%u\n", -(long)(n - 1 - i) * LOG_PERIODO_MS,   // 0 = ultima amostra
                  (int)a.erro * 50, (int)a.esq * 2, (int)a.dir * 2,
                  (a.bits & LOG_VISTA) ? 1u : 0u, (a.bits & LOG_CRUZ) ? 1u : 0u,
                  (a.bits & LOG_RETA) ? 1u : 0u);
  }
}

// ---------------------------------------------------------------- ao vivo

static bool interrompido() {
  return Con.available() > 0 || hwBotao(1) || hwBotao(2);
}

static void esvaziarEntrada() {
  delay(20);
  while (Con.available() > 0) Con.read();
  while (hwBotao(1) || hwBotao(2)) delay(1);
  delay(30);
}

// Os N_SENSORES (0..255, 255 = linha), erro, L = linha vista, X = faixa atravessada
static void fluxoNormalizado() {
  if (!barra.calibrado()) {
    Con.println(F("calibre antes"));
    return;
  }
  corrida.preparar();
  uint8_t norm[N_SENSORES];
  while (!interrompido()) {
    for (uint8_t k = 0; k < 50 && !interrompido(); k++) {
      hwLerBarra(norm);
      corrida.observar(norm);
      delay(2);
    }
    for (uint8_t i = 0; i < N_SENSORES; i++) Con.printf("%u\t", (unsigned)corrida.linha().norm(i));
    Con.printf("erro %d\t%c%c\n", (int)corrida.linha().erro(), corrida.linha().vista() ? 'L' : '-',
                  corrida.linha().cruzamento() ? 'X' : '-');
  }
  esvaziarEntrada();
}

// Cru (12 bits), sem depender do mapa: C0..C15 do mux e o pino do IR
static void fluxoBruto() {
  Con.println(F("C0..C15 (12 bits) | IR (1 = sem portadora)"));
  while (!interrompido()) {
    for (uint8_t c = 0; c < NUM_CANAIS; c++) {
      Con.printf(c == NUM_CANAIS - 1 ? "%u\t| " : "%u\t", (unsigned)barra.lerCanal(c));
    }
    Con.println(hwIrNivel() ? 1 : 0);
    for (uint8_t k = 0; k < 20 && !interrompido(); k++) delay(10);
  }
  esvaziarEntrada();
}

// Mesma conferencia do comando 'm' do teste_barra, resumida
static void conferirCanais() {
  Con.println(F("passe a mao sobre TODA a barra por 8 s..."));
  barra.iniciarMapeamento();
  uint32_t t0 = millis();
  while (millis() - t0 < 8000) barra.amostrarMapeamento();
  const ResultadoMapa &r = barra.finalizarMapeamento();
  Con.print(F("canais ativos:"));
  for (uint8_t i = 0; i < r.nAtivos; i++) Con.printf(" C%u", (unsigned)r.ativos[i]);
  Con.print(F("\nmapa (barra.h):"));
  for (uint8_t i = 0; i < N_SENSORES; i++) Con.printf(" C%u", (unsigned)barra.canalDoSensor(i));
  Con.println(r.confere ? F("\nOK: os canais ativos sao os do mapa")
                           : F("\nNAO CONFERE: veja o comando 'm' do teste_barra para o detalhe"));
}

// Quadros RC5 e NEC recebidos e o que significam para este robo
static void fluxoIR() {
  consoleArena();
  Con.println(F("aperte os botoes do controle (qualquer tecla para)"));
  const RegrasIR r = regrasIR();
  QuadroIR q;
  hwIrQuadro(q);   // descarta um quadro antigo
  while (!interrompido()) {
    if (!hwIrQuadro(q)) {
      delay(1);
      continue;
    }
    Con.printf("%s end %u cmd %u T %u", q.protocolo == IR_NEC ? "NEC" : "RC5", (unsigned)q.endereco,
                  (unsigned)q.comando, (unsigned)q.toggle);
    switch (interpretarIR(q, r)) {
      case IR_PARTIDA:  Con.println(F("  = PARTIDA")); break;
      case IR_PARADA:   Con.println(F("  = PARADA")); break;
      case IR_PROGRAMA: Con.println(F("  = PROGRAMAR (aqui nao grava; use fora do 'ir')")); break;
      default:
        Con.println(q.protocolo == IR_NEC && !P.nec ? F("  (NEC desligado: nec=1 para usar)")
                                                       : F("  (outra arena ou outro aparelho)"));
        break;
    }
  }
  esvaziarEntrada();
}

// Esquerda frente, esquerda re, direita frente, direita re (1 a 4 bipes antes de cada)
void testeMotores() {
  Con.println(F("teste das rodas: PWM 120 de 255, 0,5 s cada"));
  for (uint8_t k = 0; k < 4; k++) {
    bool esq = (k < 2);
    int16_t v = (k & 1) ? -120 : 120;   // 40 nao vencia o atrito dos motores (18/11)
    Con.print(esq ? F("roda ESQUERDA ") : F("roda DIREITA "));
    Con.println(v > 0 ? F("para a frente") : F("para tras"));
    bipe(k + 1, 50, BIPE_AGUDO);
    delay(300);
    hwMotores(esq ? v : 0, esq ? 0 : v);
    delay(500);
    hwMotores(0, 0);
    delay(700);
  }
}

// EDF no nivel atual por 3 s (rampa de 0,5 s); qualquer tecla ou botao para
void testeEdf() {
  const uint8_t alvo = ventDoNivel(P, nivelAtual);
  Con.printf("EDF em %u (de 255) por 3 s em 1 s; SW2 tem de estar ligada. Qualquer tecla para.\n",
                (unsigned)alvo);
  bipe(3, 60, BIPE_AGUDO);
  delay(1000);
  esvaziarEntrada();
  const uint32_t t0 = millis();
  while (millis() - t0 < 3000 && !interrompido()) {
    uint32_t t = millis() - t0;
    hwVentoinha(t < 500 ? (uint8_t)((uint32_t)alvo * t / 500) : alvo);
    delay(5);
  }
  hwVentoinha(0);
  esvaziarEntrada();
  Con.println(F("EDF desligada"));
}

// ---------------------------------------------------------------- comandos

static void executar(char *cmd) {
  char *igual = strchr(cmd, '=');
  if (igual) {
    *igual = 0;
    definir(cmd, igual + 1);
  } else if (!strcmp(cmd, "?")) {
    ajuda();
  } else if (!strcmp(cmd, "p")) {
    listarParametros();
  } else if (!strcmp(cmd, "salvar")) {
    Con.println(memSalvarParametros(P) ? F("parametros gravados") : F("ERRO ao gravar na NVS"));
  } else if (!strcmp(cmd, "padrao")) {
    parametrosPadrao(P);
    nivelAtual = P.nivel;
    listarParametros();
  } else if (!strcmp(cmd, "cor")) {
    barra.setCorLinha(barra.corLinha() == LINHA_BRANCA ? LINHA_PRETA : LINHA_BRANCA);
    Con.printf("linha %s ate desligar (para mudar de vez: COR_LINHA_PADRAO no config.h)\n",
                  barra.corLinha() == LINHA_BRANCA ? "BRANCA" : "PRETA");
  } else if (!strcmp(cmd, "s")) {
    fluxoNormalizado();
  } else if (!strcmp(cmd, "r")) {
    fluxoBruto();
  } else if (!strcmp(cmd, "m")) {
    conferirCanais();
  } else if (!strcmp(cmd, "ir")) {
    fluxoIR();
  } else if (!strcmp(cmd, "res")) {
    consoleResumo();
  } else if (!strcmp(cmd, "log")) {
    imprimirLog();
  } else if (!strcmp(cmd, "motores")) {
    testeMotores();
  } else if (!strcmp(cmd, "edf")) {
    testeEdf();
  } else {
    Con.println(F("? para ajuda"));
  }
}

void consoleProcessar() {
  static char linha[28];
  static uint8_t n = 0;
  static bool estourou = false;

  while (Con.available() > 0) {
    char ch = (char)Con.read();
    if (ch == '\r' || ch == '\n') {
      if (n > 0 && !estourou) {
        linha[n] = 0;
        executar(linha);
      }
      n = 0;
      estourou = false;
    } else if (ch == ' ') {
      // ignora espacos ("kp = 0.05")
    } else if (ch < ' ' || ch > '~') {
      estourou = true;   // lixo na linha: descarta
    } else if (n < sizeof(linha) - 1) {
      linha[n++] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
    } else {
      estourou = true;
    }
  }
}
