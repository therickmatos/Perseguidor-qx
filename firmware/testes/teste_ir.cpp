/*
 * teste_ir.cpp - confere os decodificadores RC5 e NEC (ir.cpp) no PC
 *
 * Copiado do firmware do perseguidor RUSH - Dreamcast (therickmatos/Perseguidor-de-linha),
 * baseado no EVA-RT 1.2.2 (c) 2025 EXOTIC TEAM MX, licenca AGPL-3.0.
 *
 * Gera a saida do receptor (alto = sem portadora) para quadros RC5 e NEC, com
 * o que o receptor e a interrupcao fazem com eles: pulsos com portadora mais
 * longos ou mais curtos, atraso variavel da interrupcao e a resolucao de 4 us
 * do micros(). As bordas passam pelos dois decodificadores, como no robo.
 * Confere tambem quadros estragados, ruido e o significado de cada quadro.
 *
 * Rodar nesta pasta:
 *   g++ -O2 -std=c++11 -I../principal teste_ir.cpp ../principal/ir.cpp -o teste_ir && ./teste_ir
 */
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "ir.h"

static int falhas = 0;
static void conferir(bool ok, const char *o_que) {
  if (!ok) { falhas++; std::printf("    FALHOU: %s\n", o_que); }
}

static unsigned semente = 2024;
static double aleatorio() {   // 0..1
  semente = semente * 1103515245u + 12345u;
  return ((semente >> 16) & 0x7FFF) / 32767.0;
}

struct Borda { double t; bool nivel; };

// Bordas de um quadro a partir de t0 (inicio do S1). distorcao: quanto o
// receptor alarga (+) ou encurta (-) cada pulso com portadora, em us.
static void quadro(std::vector<Borda> &b, double t0, uint8_t end, uint8_t cmd, uint8_t tog,
                   double distorcao) {
  const double MEIO = 889.0;
  uint16_t bits = (uint16_t)(1u << 13) | (uint16_t)(((cmd & 0x40) ? 0 : 1) << 12) |
                  (uint16_t)((tog & 1) << 11) | (uint16_t)((end & 0x1F) << 6) | (cmd & 0x3F);
  const size_t antes = b.size();
  bool nivel = true;   // linha parada (sem portadora)
  for (int i = 13; i >= 0; i--) {
    bool um = (bits >> i) & 1;
    bool primeira = um;          // "1": sem portadora e depois com
    double tb = t0 + (13 - i) * 2 * MEIO;
    if (primeira != nivel) { b.push_back({tb, primeira}); nivel = primeira; }
    b.push_back({tb + MEIO, !primeira});
    nivel = !primeira;
  }
  if (!nivel) b.push_back({t0 + 14 * 2 * MEIO, true});
  // o receptor atrasa a subida (fim da portadora) em relacao a descida
  for (size_t i = antes; i < b.size(); i++) if (b[i].nivel) b[i].t += distorcao;
}

// Bordas de um quadro NEC a partir de t0 (inicio do lider). Endereco de 16
// bits: se 'estendido' for falso, o byte de cima e o inverso do de baixo.
static void quadroNEC(std::vector<Borda> &b, double t0, uint16_t end, bool estendido, uint8_t cmd,
                      double distorcao) {
  const size_t antes = b.size();
  uint32_t bits = estendido ? end : (uint32_t)((end & 0xFF) | ((~end & 0xFF) << 8));
  bits |= (uint32_t)cmd << 16 | (uint32_t)(uint8_t)~cmd << 24;
  double t = t0;
  b.push_back({t, false});
  t += 9000;
  b.push_back({t, true});
  t += 4500;
  for (int i = 0; i < 32; i++) {
    b.push_back({t, false});
    t += 562.5;
    b.push_back({t, true});
    t += ((bits >> i) & 1) ? 1687.5 : 562.5;
  }
  b.push_back({t, false});   // pulso final
  b.push_back({t + 562.5, true});
  for (size_t i = antes; i < b.size(); i++) if (b[i].nivel) b[i].t += distorcao;
}

// Codigo de repeticao NEC (botao segurado)
static void repeticaoNEC(std::vector<Borda> &b, double t0, double distorcao) {
  b.push_back({t0, false});
  b.push_back({t0 + 9000 + distorcao, true});
  b.push_back({t0 + 11250, false});
  b.push_back({t0 + 11812.5 + distorcao, true});
}

// Passa as bordas pelos dois decodificadores, como a interrupcao faz, e
// devolve os quadros
static std::vector<QuadroIR> decodificar(std::vector<Borda> b, double atrasoMax, uint32_t base = 0) {
  std::vector<QuadroIR> saida;
  DecodificadorRC5 d;
  DecodificadorNEC n;
  d.reiniciar();
  n.reiniciar();
  for (const Borda &x : b) {
    double t = x.t + atrasoMax * aleatorio();
    uint32_t us = base + ((uint32_t)t & ~3u);   // micros() anda de 4 em 4
    if (d.borda(us, x.nivel)) saida.push_back(d.quadro());
    if (n.borda(us, x.nivel)) saida.push_back(n.quadro());
  }
  return saida;
}

static RegrasIR regras(uint8_t arena, bool qualquer, bool nec) {
  RegrasIR r;
  r.arena = arena;
  r.paradaQualquer = qualquer;
  r.endPrograma = 0x0B;
  r.endPartida = 0x07;
  r.nec = nec;
  r.necEndereco = 0x00;
  r.necPartida = 0x45;
  r.necParada = 0x46;
  return r;
}

int main() {
  // ---------------------------------------------------------------- 1. todos os comandos
  const double distorcoes[] = {-200, -100, 0, 100, 200, 300};
  const uint8_t enderecos[] = {0, 7, 11, 31};
  long total = 0, certos = 0;
  for (double dist : distorcoes)
    for (uint8_t end : enderecos)
      for (int cmd = 0; cmd < 128; cmd++)
        for (int tog = 0; tog < 2; tog++) {
          std::vector<Borda> b;
          quadro(b, 5000, end, (uint8_t)cmd, (uint8_t)tog, dist);
          std::vector<QuadroIR> q = decodificar(b, 30);
          total++;
          if (q.size() == 1 && q[0].protocolo == IR_RC5 && q[0].endereco == end && q[0].comando == cmd &&
              q[0].toggle == tog) certos++;
        }
  std::printf("1. RC5, quadros isolados (distorcao -200..+300 us, atraso ate 30 us): %ld de %ld certos\n",
              certos, total);
  conferir(certos == total, "quadro isolado mal decodificado");

  // ---------------------------------------------------------------- 2. repeticao
  {
    std::vector<Borda> b;
    for (int k = 0; k < 10; k++) quadro(b, 5000 + k * 113778.0, 7, 1, 0, 150);
    std::vector<QuadroIR> q = decodificar(b, 30);
    std::printf("2. RC5, botao segurado (10 quadros a cada 113,8 ms): %zu decodificados\n", q.size());
    conferir(q.size() == 10, "perdeu repeticao");
  }

  // ---------------------------------------------------------------- 3. volta do micros()
  {
    std::vector<Borda> b;
    quadro(b, 5000, 7, 21, 1, 100);
    std::vector<QuadroIR> q = decodificar(b, 30, 0xFFFFFFFFu - 20000u);
    std::printf("3. RC5, micros() dando a volta no meio do quadro: %s\n",
                (q.size() == 1 && q[0].comando == 21) ? "ok" : "erro");
    conferir(q.size() == 1 && q[0].endereco == 7 && q[0].comando == 21, "volta do micros()");
  }

  // ---------------------------------------------------------------- 4. quadro estragado
  // Um pico de 100 us no meio de cada um dos bits: o quadro pode se perder,
  // mas nunca virar outro quadro. Depois dele vem um bom, que tem de passar.
  {
    int trocados = 0, perdidos = 0, bonsDepois = 0;
    for (int bit = 0; bit < 14; bit++) {
      std::vector<Borda> b;
      quadro(b, 5000, 7, 1, 0, 0);
      double tp = 5000 + bit * 1778 + 400;
      // pico: inverte o nivel por 100 us
      bool nivel = true;
      for (const Borda &x : b) if (x.t <= tp) nivel = x.nivel;
      b.push_back({tp, !nivel});
      b.push_back({tp + 100, nivel});
      std::vector<Borda> ord;
      for (const Borda &x : b) {
        size_t i = 0;
        while (i < ord.size() && ord[i].t <= x.t) i++;
        ord.insert(ord.begin() + i, x);
      }
      quadro(ord, 5000 + 113778.0, 7, 0, 1, 0);
      std::vector<QuadroIR> q = decodificar(ord, 0);
      bool primeiroCerto = !q.empty() && q[0].endereco == 7 && q[0].comando == 1;
      bool ultimoCerto = !q.empty() && q.back().endereco == 7 && q.back().comando == 0 && q.back().toggle == 1;
      if (!q.empty() && !primeiroCerto && !(q.size() == 1 && ultimoCerto)) trocados++;
      if (q.size() < 2) perdidos++;
      if (ultimoCerto) bonsDepois++;
    }
    std::printf("4. RC5, pico de 100 us em cada bit: %d viraram outro quadro, %d perdidos, %d de 14 bons depois\n",
                trocados, perdidos, bonsDepois);
    conferir(trocados == 0, "pico virou outro quadro");
    conferir(bonsDepois == 14, "o quadro bom seguinte se perdeu");
  }

  // ---------------------------------------------------------------- 5. quadro cortado
  {
    std::vector<Borda> b, c;
    quadro(b, 5000, 7, 1, 0, 0);
    for (size_t i = 0; i < b.size() / 2; i++) c.push_back(b[i]);
    if (!c.back().nivel) c.push_back({c.back().t + 300, true});
    quadro(c, 40000, 7, 0, 1, 0);
    std::vector<QuadroIR> q = decodificar(c, 0);
    std::printf("5. RC5, meio quadro e depois um inteiro: %zu decodificado(s)\n", q.size());
    conferir(q.size() == 1 && q[0].comando == 0 && q[0].toggle == 1, "quadro cortado");
  }

  // ---------------------------------------------------------------- 6. NEC
  {
    long tot = 0, ok = 0;
    const uint16_t ends[] = {0x00, 0x04, 0xEF};
    for (double dist : distorcoes)
      for (uint16_t end : ends)
        for (int cmd = 0; cmd < 256; cmd++) {
          std::vector<Borda> b;
          quadroNEC(b, 5000, end, false, (uint8_t)cmd, dist);
          std::vector<QuadroIR> q = decodificar(b, 30);
          tot++;
          if (q.size() == 1 && q[0].protocolo == IR_NEC && q[0].endereco == end && q[0].comando == cmd) ok++;
        }
    std::printf("6. NEC, quadros isolados (distorcao -200..+300 us): %ld de %ld certos\n", ok, tot);
    conferir(ok == tot, "quadro NEC mal decodificado");

    std::vector<Borda> b;
    quadroNEC(b, 5000, 0x3A5C, true, 0x45, 150);
    std::vector<QuadroIR> q = decodificar(b, 30);
    conferir(q.size() == 1 && q[0].endereco == 0x3A5C && q[0].comando == 0x45, "NEC estendido");

    // botao segurado: um quadro e depois codigos de repeticao a cada 108 ms
    b.clear();
    quadroNEC(b, 5000, 0x00, false, 0x46, 150);
    for (int k = 1; k <= 6; k++) repeticaoNEC(b, 5000 + k * 108000.0, 150);
    quadroNEC(b, 5000 + 7 * 108000.0, 0x00, false, 0x45, 150);
    q = decodificar(b, 30);
    std::printf("   NEC, botao segurado (1 quadro + 6 repeticoes) e outra tecla: %zu decodificados\n", q.size());
    conferir(q.size() == 2 && q[0].comando == 0x46 && q[1].comando == 0x45, "repeticao NEC");

    // comando com o inverso errado: o bit 1 do comando (0x45) vira 1 alongando o
    // espaco dele; o inverso continua o de 0x45 e o quadro tem de ser descartado.
    // Endereco 0x00: 8 bits de 1125 us e 8 de 2250; bit 0 do comando: 2250.
    b.clear();
    quadroNEC(b, 5000, 0x00, false, 0x45, 0);
    const double espacoBit1 = 5000 + 13500 + 8 * 1125 + 8 * 2250 + 2250 + 562.5;
    for (Borda &x : b) if (x.t > espacoBit1 + 1) x.t += 1125;
    q = decodificar(b, 0);
    conferir(q.empty(), "NEC com o inverso errado foi aceito");

    // um decodificador nao enxerga o protocolo do outro
    b.clear();
    for (int k = 0; k < 50; k++) {
      quadro(b, 5000 + k * 230000.0, 7, (uint8_t)(k * 5), (uint8_t)(k & 1), 100);
      quadroNEC(b, 5000 + k * 230000.0 + 110000, 0x00, false, (uint8_t)(k * 3), 100);
    }
    q = decodificar(b, 30);
    int rc5 = 0, nec = 0;
    for (const QuadroIR &x : q) (x.protocolo == IR_NEC ? nec : rc5)++;
    std::printf("   RC5 e NEC alternados (50 + 50): %d RC5, %d NEC\n", rc5, nec);
    conferir(rc5 == 50 && nec == 50, "um protocolo atrapalhou o outro");
  }

  // ---------------------------------------------------------------- 7. ruido
  // Pulsos aleatorios de 100 us a 3 ms (luz de camera, reflexo): conta quantos
  // quadros validos aparecem e quantos seriam partida ou parada na arena 0
  // (com o NEC ligado).
  {
    std::vector<Borda> b;
    double t = 0;
    bool nivel = true;
    while (t < 600e6) {   // 10 minutos
      t += 100 + 2900 * aleatorio();
      nivel = !nivel;
      b.push_back({t, nivel});
    }
    // e pulsos longos, que podem parecer o lider do NEC
    double t2 = 0;
    nivel = true;
    std::vector<Borda> c;
    while (t2 < 600e6) {
      t2 += 300 + 12000 * aleatorio();
      nivel = !nivel;
      c.push_back({t2, nivel});
    }
    int n = 0, partida = 0, parada = 0, programa = 0;
    for (const std::vector<Borda> *v : {&b, &c}) {
      std::vector<QuadroIR> q = decodificar(*v, 30);
      n += (int)q.size();
      for (const QuadroIR &x : q) {
        ComandoIR cmd = interpretarIR(x, regras(0, false, true));
        if (cmd == IR_PARTIDA) partida++;
        if (cmd == IR_PARADA) parada++;
        if (cmd == IR_PROGRAMA) programa++;
      }
    }
    std::printf("7. 2 x 10 min de ruido (%zu bordas): %d quadros validos; partida %d, parada %d, programar %d\n",
                b.size() + c.size(), n, partida, parada, programa);
  }

  // ---------------------------------------------------------------- 8. significado
  {
    QuadroIR q;
    q.protocolo = IR_RC5;
    q.toggle = 0;
    q.endereco = 0x0B; q.comando = 0x14;
    conferir(interpretarIR(q, regras(0, false, false)) == IR_PROGRAMA && arenaDoQuadro(q) == 0x14, "programar");
    q.endereco = 0x0B; q.comando = 0x15;
    conferir(arenaDoQuadro(q) == 0x14, "programar com numero impar");
    q.endereco = 0x07; q.comando = 0x15;
    conferir(interpretarIR(q, regras(0x14, false, false)) == IR_PARTIDA, "partida");
    q.comando = 0x14;
    conferir(interpretarIR(q, regras(0x14, false, false)) == IR_PARADA, "parada");
    q.comando = 0x16;
    conferir(interpretarIR(q, regras(0x14, false, false)) == IR_OUTRO, "parada de outra arena");
    conferir(interpretarIR(q, regras(0x14, true, false)) == IR_PARADA, "parada de qualquer arena");
    q.comando = 0x17;
    conferir(interpretarIR(q, regras(0x14, true, false)) == IR_OUTRO, "partida de outra arena");
    q.endereco = 0x00; q.comando = 0x15;
    conferir(interpretarIR(q, regras(0x14, true, false)) == IR_OUTRO, "outro aparelho");
    q.protocolo = IR_NEC; q.endereco = 0x00; q.comando = 0x45;
    conferir(interpretarIR(q, regras(0x14, false, false)) == IR_OUTRO, "NEC desligado");
    conferir(interpretarIR(q, regras(0x14, false, true)) == IR_PARTIDA, "NEC partida");
    q.comando = 0x46;
    conferir(interpretarIR(q, regras(0x14, false, true)) == IR_PARADA, "NEC parada");
    q.endereco = 0x04;
    conferir(interpretarIR(q, regras(0x14, false, true)) == IR_OUTRO, "NEC de outro endereco");
    q.endereco = 0x0B; q.comando = 0x14;
    conferir(interpretarIR(q, regras(0x14, false, true)) == IR_OUTRO, "NEC nao programa a arena");
    std::printf("8. significado dos quadros (programar, partida, parada, outra arena, NEC): conferido\n");
  }

  std::printf("\n%s\n", falhas ? "HOUVE FALHAS" : "tudo certo");
  return falhas ? 1 : 0;
}
