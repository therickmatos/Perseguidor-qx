/*
 * sim.cpp - roda o controle do firmware (linha.cpp + corrida.cpp) no PC
 *
 * Baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0
 * Copiado do simulador do perseguidor RUSH - Dreamcast (therickmatos/Perseguidor-de-linha).
 * Mudancas para o Perseguidor-qx: a leitura passa pela mesma normalizacao do
 * barra.cpp (12 bits, 0..1000) e pela conversao do hw.cpp (0..255), e o ciclo
 * e o PERIODO_US do config.h (2 ms).
 *
 * Confere a logica e os sinais (lado do erro, roda que acelera, linha de
 * partida, parada pelo comando do juiz, perda da linha) sem o robo. NAO
 * serve para ajustar ganhos nem o freio: o modelo dos motores e inventado
 * (velocidade proporcional ao PWM com atraso de primeira ordem, sem escorregar).
 *
 * Rodar nesta pasta:
 *   g++ -O2 -std=c++11 -I../principal sim.cpp ../principal/linha.cpp \
 *       ../principal/corrida.cpp -o sim && ./sim
 *
 * Pistas:
 *  - ensaio: trecho aberto com retas, R100 a R300, curvas em S e uma linha
 *    de partida no meio de uma reta (para os casos de falha);
 *  - perseguidor: circuito fechado e simetrico de 7,4 m, inventado dentro do
 *    regulamento do Perseguidor (RoboCore 12/12/2023: raio >= 100, linhas
 *    de partida de 200 mm em retas, 250 mm de reta antes e depois, 200 mm
 *    entre trechos, cabe em 3 x 3 m);
 *  - trif2025 e trif2024: modelos das pistas do seguidor (estudo de caso do
 *    repositorio do seguidor), mais dificeis: cruzamentos de verdade e curvas
 *    de R100. Aqui sem as marcas laterais, que o perseguidor nao le.
 *
 * Geometria: barra em arco de R 180 mm, 12 sensores a cada 7,8 mm (Gerber da
 * V1), 107,1 mm a frente do eixo e 110 mm entre as rodas: os numeros do
 * Dreamcast. No Perseguidor-qx a distancia depende do furo escolhido no braco
 * (mecanica/); trocar L_BARRA e BITOLA quando o robo estiver montado.
 * Linha de 19 mm.
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "corrida.h"

static const double PI = 3.14159265358979;
static const double R90 = PI / 2;

// ------------------------------------------------------------------ pista

struct Trecho { double comprimento, curvatura; };   // mm, 1/mm (+ = esquerda)
struct Ponto  { double x, y, th; };
// Faixa atravessada em s: linha de partida (meia = 100 mm para cada lado) ou
// cruzamento (o outro ramo, comprido). tangente: inclinacao em relacao a 90 graus.
struct Faixa  { double s, tangente, meia; };

struct Pista {
  std::vector<Ponto> pt;      // a cada PASSO mm
  std::vector<Faixa> faixas;
  double fim;                 // a linha acaba aqui (mm)
};
static const double PASSO = 0.5;

static Pista montar(const std::vector<Trecho> &trechos) {
  Pista p;
  double x = 0, y = 0, th = 0, s = 0;
  p.pt.push_back({x, y, th});
  for (const Trecho &t : trechos) {
    int n = (int)std::lround(t.comprimento / PASSO);
    for (int i = 0; i < n; i++) {
      th += t.curvatura * PASSO;
      x += std::cos(th) * PASSO;
      y += std::sin(th) * PASSO;
      p.pt.push_back({x, y, th});
    }
    s += t.comprimento;
  }
  p.fim = s;
  return p;
}

// Circuito fechado desenrolado em varias voltas
struct Circuito {
  Pista pista;
  double volta;
  std::vector<double> partidas;   // s das linhas de partida numa volta
};

static Circuito desenrolar(const std::vector<Trecho> &umaVolta, int voltas,
                           const std::vector<Faixa> &faixasDaVolta) {
  Circuito c;
  c.volta = 0;
  for (const Trecho &t : umaVolta) c.volta += t.comprimento;
  std::vector<Trecho> tudo;
  for (int v = 0; v < voltas; v++) tudo.insert(tudo.end(), umaVolta.begin(), umaVolta.end());
  c.pista = montar(tudo);
  for (int v = 0; v < voltas; v++)
    for (const Faixa &f : faixasDaVolta) c.pista.faixas.push_back({f.s + v * c.volta, f.tangente, f.meia});
  return c;
}

// Pista de pistas/*.txt (formato do simulador do seguidor). As marcas P e C
// sao ignoradas (o perseguidor nao tem sensores de marca); X vira cruzamento.
static bool carregar(const std::string &arquivo, int voltas, Circuito &c, double &partida) {
  FILE *f = std::fopen(arquivo.c_str(), "r");
  if (!f) return false;
  std::vector<Trecho> umaVolta;
  std::vector<Faixa> cruz;
  partida = 0;
  char linha[200];
  while (std::fgets(linha, sizeof(linha), f)) {
    double a = 0, b = 0;
    if (std::sscanf(linha, "T %lf %lf", &a, &b) == 2) umaVolta.push_back({a, b});
    else if (std::sscanf(linha, "P %lf", &a) == 1) partida = a;
    else if (std::sscanf(linha, "X %lf %lf", &a, &b) == 2) cruz.push_back({a, b, 400});
  }
  std::fclose(f);
  c = desenrolar(umaVolta, voltas, cruz);
  return true;
}

static double BORDA = 3.0;   // largura da borda borrada vista pelo sensor (mm)

static double rampa(double dentro, double borda) {   // 1 dentro, 0 fora
  double v = dentro / borda + 0.5;
  return v < 0 ? 0 : (v > 1 ? 1 : v);
}

// Indice do ponto da linha mais proximo de (px, py), procurando de 'de' a 'ate'
static int maisProximo(const Pista &p, double px, double py, int de, int ate) {
  int n = (int)p.pt.size();
  if (de < 0) de = 0;
  if (ate >= n) ate = n - 1;
  double melhor = 1e18;
  int j = de;
  for (int i = de; i <= ate; i++) {
    double dx = px - p.pt[i].x, dy = py - p.pt[i].y;
    double d2 = dx * dx + dy * dy;
    if (d2 < melhor) { melhor = d2; j = i; }
  }
  return j;
}

// Quanto de branco ha embaixo do ponto (0..1). 'eixo' e o indice do ponto da
// linha mais proximo do eixo do robo; devolve em 'j' o mais proximo do ponto.
static double branco(const Pista &p, double px, double py, int eixo, int &j) {
  j = maisProximo(p, px, py, eixo - 300, eixo + 600);   // -150 a +300 mm
  const Ponto &q = p.pt[j];
  double dx = px - q.x, dy = py - q.y;
  double lado = -std::sin(q.th) * dx + std::cos(q.th) * dy;      // + = esquerda
  double s = j * PASSO + std::cos(q.th) * dx + std::sin(q.th) * dy;

  const double B = BORDA;
  double w = 0;
  if (s <= p.fim) w = rampa(9.5 - std::fabs(lado), B);
  for (const Faixa &f : p.faixas) {
    double v = std::fmin(rampa(9.5 - std::fabs(s - f.s - lado * f.tangente), B),
                         rampa(f.meia - std::fabs(lado), B));
    if (v > w) w = v;
  }
  return w;
}

// ------------------------------------------------------------------ robo

static const double L_BARRA = 107.1, R_ARCO = 180.0, PASSO_ANG = 2.4826 * PI / 180.0;
static const double BITOLA = 110.0;          // entre as rodas (mm)
static double MM_S_POR_PWM = 45.0;           // PWM 40 -> 1,8 m/s
static double TAU_MOTOR = 0.120;             // s

struct Robo {
  double x, y, th, vE, vD;
  int eixo;              // indice do ponto da linha mais proximo do eixo
};

// Leitura bruta de cada sensor no preto e no branco, em 8 bits como no
// simulador do Dreamcast. Nesta barra a leitura sobe com a reflexao (branco
// alto, medido no robo em 24/11/2026). O ADC do ESP32-C3 e de 12 bits:
// ler() multiplica por 16.
static double PRETO[N_SENSORES], BRANCO[N_SENSORES];
static double ESCALA_BAT = 256;

static void sensoresIguais() {
  for (int i = 0; i < N_SENSORES; i++) {
    PRETO[i] = 40;
    BRANCO[i] = 210;
  }
}

// O que o robo faz com uma leitura crua de 12 bits: Barra::normalizar()
// (0..1000, 1000 = linha; linha preta inverte) e a conversao de hwLerBarra()
// para 0..255. Manter igual ao barra.cpp e ao hw.cpp.
static uint16_t CAL_MIN[N_SENSORES], CAL_MAX[N_SENSORES];
static uint8_t normalizarComoRobo(int i, uint16_t cru) {
  int32_t v = ((int32_t)cru - CAL_MIN[i]) * 1000 / ((int32_t)CAL_MAX[i] - CAL_MIN[i]);
  if (v < 0) v = 0;
  else if (v > 1000) v = 1000;
  return (uint8_t)((v * 255 + 500) / 1000);
}

static unsigned semente = 12345;
static double ruido() {   // -1..1
  semente = semente * 1103515245u + 12345u;
  return ((semente >> 16) & 0x7FFF) / 16383.5 - 1.0;
}

static void ler(const Pista &p, Robo &r, uint8_t norm[N_SENSORES]) {
  double c = std::cos(r.th), s = std::sin(r.th);
  r.eixo = maisProximo(p, r.x, r.y, r.eixo - 200, r.eixo + 200);
  for (int i = 0; i < N_SENSORES; i++) {
    double a = (i - (N_SENSORES - 1) / 2.0) * PASSO_ANG;   // sensor 0 = esquerda
    double direita = R_ARCO * std::sin(a);
    double frente = L_BARRA - R_ARCO * (1 - std::cos(a));
    double px = r.x + frente * c + direita * s;
    double py = r.y + frente * s - direita * c;
    int j;
    double w = branco(p, px, py, r.eixo, j);
    double v = 16 * (PRETO[i] + (BRANCO[i] - PRETO[i]) * w + 3 * ruido());
    norm[i] = normalizarComoRobo(i, (uint16_t)(v < 0 ? 0 : (v > 4095 ? 4095 : v)));
  }
}

static void andar(Robo &r, const Saida &s, double dt) {
  r.vE += (MM_S_POR_PWM * s.esq - r.vE) * dt / TAU_MOTOR;
  r.vD += (MM_S_POR_PWM * s.dir - r.vD) * dt / TAU_MOTOR;
  double v = (r.vE + r.vD) / 2, w = (r.vD - r.vE) / BITOLA;
  r.th += w * dt;
  r.x += v * std::cos(r.th) * dt;
  r.y += v * std::sin(r.th) * dt;
}

// ------------------------------------------------------------------ ensaio

struct Resultado {
  Resumo r;
  double desvioMax;     // maior distancia do centro da barra a linha (mm)
  double desvioFaixa;   // idem, so com a barra perto de uma faixa atravessada
  double vMax;          // m/s
  double sParou;        // posicao do eixo ao parar (mm ao longo da pista)
  double depoisParada;  // mm andados entre o comando de parada e o fim
  double re;            // mm andados para tras depois da parada
  double tParada;       // s do comando de parada (desde a largada)
  unsigned faixasEsperadas;   // faixas que a barra cruzou ate a parada
};

// sEixo: onde fica o eixo das rodas na largada (mm ao longo da pista).
// sParar: o "juiz" manda parar quando o eixo passa daqui (< 0 = nunca).
// esperaMs: 0 = largada pelo modulo de inicio; 1000 = pelo botao.
// tPararMs: alternativa a sParar, por tempo (< 0 = nao usa).
static Resultado ensaiar(const Pista &p, const Parametros &par, uint8_t nivel,
                         double sEixo, double desvioInicial, double anguloInicial,
                         double sParar, uint16_t esperaMs = 0, double tPararMs = -1) {
  // calibracao: tudo preto e tudo branco (minimo e maximo de cada sensor)
  for (int i = 0; i < N_SENSORES; i++) {
    CAL_MIN[i] = (uint16_t)(16 * PRETO[i]);
    CAL_MAX[i] = (uint16_t)(16 * BRANCO[i]);
    if (CAL_MAX[i] - CAL_MIN[i] < 150) { std::printf("calibracao falhou\n"); std::exit(2); }
  }

  Robo r = {};
  int i0 = (int)(sEixo / PASSO);
  r.th = p.pt[i0].th + anguloInicial;
  r.x = p.pt[i0].x - std::sin(p.pt[i0].th) * desvioInicial;
  r.y = p.pt[i0].y + std::cos(p.pt[i0].th) * desvioInicial;
  r.eixo = i0;

  static Corrida c;
  c.iniciar(par, nivel, esperaMs);
  Resultado res = {};
  res.tParada = -1;
  uint8_t bruto[N_SENSORES];
  Saida s;
  int naBarra = i0;
  bool parou = false;
  double xParada = 0, yParada = 0;
  const double sBarra0 = sEixo + L_BARRA;
  for (long ciclo = 0; ciclo < 400000 && c.fase() != FASE_FIM; ciclo++) {
    ler(p, r, bruto);
    if (!parou && ((sParar >= 0 && r.eixo * PASSO >= sParar) ||
                   (tPararMs >= 0 && ciclo * PERIODO_US / 1000.0 >= tPararMs))) {
      parou = true;
      c.parar();
      xParada = r.x;
      yParada = r.y;
      res.tParada = ciclo * PERIODO_US * 1e-6;
      double sb = r.eixo * PASSO + L_BARRA;
      for (const Faixa &f : p.faixas)
        if (f.s >= sBarra0 - 12 && f.s <= sb) res.faixasEsperadas++;
    }
    c.passo(bruto, (uint16_t)ESCALA_BAT, s);
    double vAntes = (r.vE + r.vD) / 2;
    for (int k = 0; k < 10; k++) andar(r, s, PERIODO_US * 1e-6 / 10);

    if (c.fase() == FASE_CORRENDO) {
      double bx = r.x + L_BARRA * std::cos(r.th), by = r.y + L_BARRA * std::sin(r.th);
      branco(p, bx, by, r.eixo, naBarra);
      double d = std::hypot(bx - p.pt[naBarra].x, by - p.pt[naBarra].y);
      if (d > res.desvioMax && naBarra * PASSO < p.fim - 1) res.desvioMax = d;
      for (const Faixa &f : p.faixas) {
        double ds = naBarra * PASSO - f.s;
        if (ds > -60 && ds < 250 && d > res.desvioFaixa) res.desvioFaixa = d;
      }
      double vel = (r.vE + r.vD) / 2000.0;
      if (vel > res.vMax) res.vMax = vel;
    }
    if (parou) {
      double vm = (vAntes + (r.vE + r.vD) / 2) / 2;
      if (vm < 0) res.re += -vm * PERIODO_US * 1e-6;
    }
  }
  // o robo ainda anda um pouco depois de a partida acabar (sem PWM, o motor
  // em curto freia): deixa assentar
  Saida zero = {0, 0, 0};
  for (int k = 0; k < 3000; k++) {
    double vAntes = (r.vE + r.vD) / 2;
    andar(r, zero, 1e-3);
    if (parou) {
      double vm = (vAntes + (r.vE + r.vD) / 2) / 2;
      if (vm < 0) res.re += -vm * 1e-3;
    }
  }
  res.r = c.resumo();
  res.sParou = maisProximo(p, r.x, r.y, r.eixo - 400, r.eixo + 400) * PASSO;
  if (parou) res.depoisParada = std::hypot(r.x - xParada, r.y - yParada);
  return res;
}

static int falhas = 0;
static void conferir(bool ok, const char *o_que) {
  if (!ok) { falhas++; std::printf("    FALHOU: %s\n", o_que); }
}

static const char *MOTIVO[] = {"nenhum", "PARADA", "PERDEU", "BOTAO", "TEMPO"};

static void mostrar(const char *nome, const Resultado &x) {
  std::printf("%-36s %-7s andou %6.3f s | vmax %.2f m/s | reta %2ld %% | desvio max %4.1f mm | "
              "faixas %u | sem linha %u ms",
              nome, MOTIVO[x.r.motivo], x.r.totalMs / 1000.0, x.vMax,
              x.r.totalMs ? (long)(x.r.retaMs * 100 / x.r.totalMs) : 0, x.desvioMax,
              x.r.faixas, x.r.perdidoMs);
  if (x.tParada >= 0) std::printf(" | parada: +%.0f mm, re %.0f mm", x.depoisParada, x.re);
  std::printf("\n");
}

// Parada pelo juiz: parou, contou as faixas certas. Quanto anda depois do
// comando (e se volta para tras) depende do motor, que aqui e inventado: so
// avisa. No robo, isso se acerta com 'freio' e 'tfreio'.
static void conferirParada(const Resultado &x) {
  conferir(x.r.motivo == FIM_PARADA, "tinha que parar pelo comando de parada");
  conferir(x.r.perdidoMs == 0, "perdeu a linha");
  conferir(x.r.faixas == x.faixasEsperadas, "contou errado as linhas de partida");
  conferir(x.depoisParada < 400, "nao parou depois do comando");
  if (x.re > 15) std::printf("    aviso: voltou %.0f mm depois de frear (com este motor, diminuir tfreio)\n", x.re);
}

int main(int argc, char **argv) {
  const std::string pasta = argc > 1 ? argv[1] : "pistas";
  Parametros par;
  parametrosPadrao(par);
  Resultado x;
  sensoresIguais();

  // ================================================================ pista de ensaio
  // Retas, R100 a R300, curvas em S e uma linha de partida no meio de uma reta.
  std::vector<Trecho> t = {
    {1500, 0},
    {200 * R90, 1.0 / 200},                               // esquerda R200 90
    {500, 0},
    {100 * 2 * R90, -1.0 / 100},                          // direita R100 180
    {400, 0},
    {150 * R90, 1.0 / 150}, {150 * R90, -1.0 / 150},      // S R150
    {1200, 0},                                            // linha de partida no meio
    {300 * 2 * R90, 1.0 / 300},                           // esquerda R300 180
    {700, 0},
    {100 * R90, -1.0 / 100}, {100 * R90, 1.0 / 100},      // S R100
    {2500, 0},
  };
  Pista pista = montar(t);
  const double sLinha = 1500 + 200 * R90 + 500 + 100 * 2 * R90 + 400 + 2 * 150 * R90 + 600;
  const double sParar = pista.fim - 1500;
  pista.faixas = {{sLinha, 0.0, 100}};
  std::printf("PISTA DE ENSAIO: %.2f m, 1 linha de partida; o juiz manda parar a %.1f m\n",
              pista.fim / 1000, sParar / 1000);

  // 1. valores padrao, nivel 0, largada pelo modulo de inicio
  x = ensaiar(pista, par, 0, 150, 0, 0, sParar);
  mostrar("1 padrao, nivel 0", x);
  conferirParada(x);
  conferir(x.desvioMax < 30, "barra longe da linha");
  conferir(x.desvioFaixa < 3, "a linha de partida desviou o robo");
  const double tPadrao = x.r.totalMs;
  const double desvioReto = x.desvioFaixa;

  // 2. mesma coisa sem as melhorias de velocidade (como o EVA-RT)
  Parametros unica = par;
  unica.reta = 0;
  x = ensaiar(pista, unica, 0, 150, 0, 0, sParar);
  mostrar("2 velocidade unica (reta=0)", x);
  conferirParada(x);
  conferir(tPadrao < x.r.totalMs, "a velocidade de reta tinha que baixar o tempo");

  // 3. niveis mais altos (rapido demais para as R100 desta pista: pode perder
  //    a linha por um instante, mas tem que achar de novo)
  for (uint8_t nivel = 4; nivel <= 12; nivel += 4) {
    char nome[40];
    std::snprintf(nome, sizeof(nome), "3 nivel %u (curva %u, reta %u)", nivel,
                  velCurvaDoNivel(par, nivel), velRetaDoNivel(par, nivel));
    x = ensaiar(pista, par, nivel, 150, 0, 0, sParar);
    mostrar(nome, x);
    conferir(x.r.motivo == FIM_PARADA, "tinha que chegar ao ponto de parada");
    conferir(x.r.faixas == x.faixasEsperadas, "contou errado as linhas de partida");
  }

  // 4. largada torta: 15 mm fora do centro e 8 graus
  x = ensaiar(pista, par, 0, 150, 15, -8 * PI / 180, sParar);
  mostrar("4 largada torta", x);
  conferirParada(x);

  // 5. linha de partida torta (85 graus): nao pode desviar o robo
  Pista torta = pista;
  torta.faixas = {{sLinha, std::tan(5 * PI / 180), 100}};
  x = ensaiar(torta, par, 0, 150, 0, 0, sParar);
  mostrar("5 linha de partida a 85 graus", x);
  conferirParada(x);
  std::printf("    desvio da barra perto da linha de partida: %.1f mm (reta: %.1f mm)\n",
              x.desvioFaixa, desvioReto);
  conferir(x.desvioFaixa < desvioReto + 3, "a faixa torta desviou o robo");

  // 6. a linha acaba: tem que desligar por perda
  Pista cortada = pista;
  cortada.fim = 1200;
  x = ensaiar(cortada, par, 0, 150, 0, 0, -1);
  mostrar("6 linha interrompida", x);
  conferir(x.r.motivo == FIM_PERDEU, "tinha que desligar por perder a linha");

  // 7. sai da curva pela lateral (ganho baixo demais): vira para o lado certo e desliga
  Parametros frouxo = par;
  frouxo.kp = 0.004f;
  frouxo.kd = 0;
  x = ensaiar(pista, frouxo, 8, 150, 0, 0, -1);
  mostrar("7 ganho baixo demais", x);
  conferir(x.r.motivo == FIM_PERDEU, "tinha que desligar por perder a linha");

  // 8. largada fora da linha: nem sai
  x = ensaiar(pista, par, 0, 150, 80, 0, -1);
  mostrar("8 largada fora da linha", x);
  conferir(x.r.motivo == FIM_PERDEU && x.r.totalMs == 0, "nao podia sair");

  // 9. tempo maximo
  Parametros curto = par;
  curto.tmax = 2;
  x = ensaiar(pista, curto, 0, 150, 0, 0, -1);
  mostrar("9 tmax = 2 s", x);
  conferir(x.r.motivo == FIM_TEMPO && x.r.totalMs == 2000, "tinha que desligar em 2 s");

  // 10. largada pelo botao (1 s) e parada antes de sair: nem anda
  x = ensaiar(pista, par, 0, 150, 0, 0, -1, 1000, 500);
  mostrar("10 parada antes de sair (botao)", x);
  conferir(x.r.motivo == FIM_PARADA && x.r.totalMs == 0 && x.depoisParada < 1,
           "tinha que desligar sem andar");

  // 11. largada pelo botao: sai depois de 1 s
  x = ensaiar(pista, par, 0, 150, 0, 0, sParar, 1000);
  mostrar("11 largada pelo botao (1 s)", x);
  conferirParada(x);

  // ================================================================ circuito do perseguidor
  // Meia volta, repetida girada de 180 graus: as duas linhas de partida ficam
  // no meio das retas de 900 mm (450 mm de reta antes e depois).
  const std::vector<Trecho> meia = {
    {900, 0},
    {250 * R90, 1.0 / 250}, {400, 0},
    {150 * R90, -1.0 / 150}, {100 * R90, 1.0 / 100}, {300, 0},
    {100 * R90, 1.0 / 100}, {500, 0},
    {120 * R90, -1.0 / 120}, {120 * R90, 1.0 / 120}, {300, 0},
  };
  std::vector<Trecho> volta = meia;
  volta.insert(volta.end(), meia.begin(), meia.end());
  double meiaVolta = 0;
  for (const Trecho &m : meia) meiaVolta += m.comprimento;
  const std::vector<Faixa> partidas = {{450, 0, 100}, {450 + meiaVolta, 0, 100}};
  const int VOLTAS = 3;
  Circuito circ = desenrolar(volta, VOLTAS + 1, partidas);
  std::printf("\nPERSEGUIDOR (circuito inventado): volta de %.2f m, 2 linhas de partida; "
              "o juiz manda parar depois de %d voltas\n", circ.volta / 1000, VOLTAS);

  // Onde o robo larga: barra em cima da sua linha de partida, um pouco antes
  // ou um pouco depois (o juiz indica o ponto; a posicao exata nao esta na regra)
  const double sPar = 450 + VOLTAS * circ.volta;
  const double posicoes[] = {0, -40, 30};
  for (double dp : posicoes) {
    char nome[48];
    std::snprintf(nome, sizeof(nome), "barra %+.0f mm da linha de partida", dp);
    x = ensaiar(circ.pista, par, 0, 450 + dp - L_BARRA, 0, 0, sPar);
    mostrar(nome, x);
    conferirParada(x);
    conferir(x.desvioMax < 30, "barra longe da linha");
    conferir(x.desvioFaixa < 3, "a linha de partida desviou o robo");
  }
  // A outra posicao de largada (do adversario) e a mesma, girada: confere so uma vez
  x = ensaiar(circ.pista, par, 0, 450 + meiaVolta - L_BARRA, 0, 0, 450 + meiaVolta + (VOLTAS - 1) * circ.volta);
  mostrar("largando da outra linha de partida", x);
  conferirParada(x);

  // Tres "robos" (atraso do motor de 60, 120 e 200 ms) e os dois modos
  const double atrasos[] = {0.060, 0.120, 0.200};
  for (double atraso : atrasos) {
    TAU_MOTOR = atraso;
    double tempo[2] = {0, 0};
    for (int modo = 0; modo < 2; modo++) {
      Parametros q = par;
      if (modo == 0) q.reta = 0;   // velocidade unica, como o EVA-RT
      char rotulo[48];
      std::snprintf(rotulo, sizeof(rotulo), "motor %3.0f ms, %s", atraso * 1000,
                    modo == 0 ? "velocidade unica" : "padrao");
      x = ensaiar(circ.pista, q, 0, 450 - L_BARRA, 0, 0, sPar);
      mostrar(rotulo, x);
      conferirParada(x);
      tempo[modo] = x.tParada;
    }
    std::printf("    %d voltas: %.2f s -> %.2f s (%.0f %% menos com reta=%u)\n", VOLTAS,
                tempo[0], tempo[1], 100 * (1 - tempo[1] / tempo[0]), par.reta);
    conferir(tempo[1] < tempo[0], "a velocidade de reta tinha que baixar o tempo");
  }
  TAU_MOTOR = 0.120;

  // Niveis mais altos no circuito
  for (uint8_t nivel = 4; nivel <= 12; nivel += 4) {
    char nome[40];
    std::snprintf(nome, sizeof(nome), "nivel %u (curva %u, reta %u)", nivel,
                  velCurvaDoNivel(par, nivel), velRetaDoNivel(par, nivel));
    x = ensaiar(circ.pista, par, nivel, 450 - L_BARRA, 0, 0, sPar);
    mostrar(nome, x);
    conferir(x.r.motivo == FIM_PARADA, "tinha que chegar ao ponto de parada");
    conferir(x.r.faixas == x.faixasEsperadas, "contou errado as linhas de partida");
  }

  // Sensores desiguais (contraste de 60 a 170 contagens), borda mais borrada
  // e PWM multiplicado por 1,2 (bateria abaixo de 'vref')
  for (int i = 0; i < N_SENSORES; i++) {
    double contraste = 115 + 55 * ruido(), escuro = 45 + 20 * ruido();
    PRETO[i] = escuro;
    BRANCO[i] = escuro + contraste;
  }
  BORDA = 6.0;
  ESCALA_BAT = 307;
  x = ensaiar(circ.pista, par, 0, 450 - L_BARRA, 0, 0, sPar);
  mostrar("sensores desiguais, PWM x 1,2", x);
  conferirParada(x);
  sensoresIguais();
  BORDA = 3.0;
  ESCALA_BAT = 256;

  // ================================================================ pistas do seguidor
  const char *nomes[] = {"trif2025", "trif2024"};
  for (const char *nome : nomes) {
    Circuito c;
    double partida;
    if (!carregar(pasta + "/" + nome + ".txt", 3, c, partida)) {
      std::printf("\n%s: arquivo nao encontrado em %s/ (rode a partir de firmware/testes)\n", nome, pasta.c_str());
      falhas++;
      continue;
    }
    std::printf("\n%s (pista do seguidor, sem as marcas): volta de %.2f m, 2 passagens pelo cruzamento\n",
                nome, c.volta / 1000);
    for (double atraso : atrasos) {
      TAU_MOTOR = atraso;
      char rotulo[48];
      std::snprintf(rotulo, sizeof(rotulo), "motor %3.0f ms, 2 voltas", atraso * 1000);
      x = ensaiar(c.pista, par, 0, partida - 500, 0, 0, partida - 500 + 2 * c.volta);
      mostrar(rotulo, x);
      conferirParada(x);
    }
    TAU_MOTOR = 0.120;
  }

  std::printf("\n%s\n", falhas ? "HOUVE FALHAS" : "tudo certo");
  return falhas ? 1 : 0;
}
