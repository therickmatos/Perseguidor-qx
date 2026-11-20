/*
 * Perseguidor-qx - ajuste pelo celular (Web Bluetooth)
 * Licenca AGPL-3.0, como o firmware.
 *
 * Fala com o console de texto do firmware (firmware/principal/console.cpp)
 * pelo servico UART da Nordic (canal.cpp). Nao ha protocolo proprio: a
 * pagina manda os mesmos comandos que se digitam na USB e le as respostas.
 * Se o texto de um comando mudar no firmware, o leitor dele aqui muda junto.
 */
'use strict';

const NUS = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';   // celular -> robo
const NUS_TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';   // robo -> celular
const PEDACO = 20;                                         // bytes por escrita (MTU minimo)

// ------------------------------------------------------------------ parametros
// Mesmos nomes e faixas da TABELA do console.cpp. 'passo' e o do + e -.
const GRUPOS = [
  ['Controle (PD)', true, [
    ['kp', 0, 2, 0.005, 'Ganho proporcional. Erro em milésimos de sensor (1000 = 7,8 mm)'],
    ['kd', 0, 20, 0.05, 'Ganho derivativo, por ms'],
    ['fd', 1, 100, 5, 'Filtro da derivada: % da leitura nova por ciclo'],
  ]],
  ['Velocidade', true, [
    ['vel', 0, 255, 5, 'PWM nas curvas fechadas, nível 0'],
    ['reta', 0, 200, 5, 'PWM a mais nas retas (0 = velocidade única)'],
    ['passo', 0, 50, 1, 'Aumento de vel e reta por nível'],
    ['nivel', 0, 40, 1, 'Nível ao ligar'],
    ['acel', 10, 20000, 50, 'PWM por segundo ao acelerar (rampa da largada)'],
    ['re', 0, 255, 5, 'Ré máxima da roda de dentro'],
  ]],
  ['Curvas e retas', false, [
    ['ecurva', 50, 5500, 50, 'Erro que indica entrada de curva: volta para vel na hora'],
    ['treta', 0, 5000, 10, 'ms com o erro quieto para poder passar de vel'],
    ['rreta', 0, 200, 1, 'Curvatura (% do PWM) até a qual anda em vel + reta'],
    ['rcurva', 0, 200, 1, 'Curvatura a partir da qual anda em vel'],
    ['fcurva', 0, 200, 5, 'Freada ao baixar a velocidade: % da queda aplicada a mais'],
    ['tfcurva', 1, 500, 5, 'Duração da freada (ms)'],
  ]],
  ['EDF', true, [
    ['vent', 0, 255, 5, 'EDF no nível 0 (0..255 = 1000..2000 µs)'],
    ['vpasso', 0, 50, 1, 'Aumento da EDF por nível'],
  ]],
  ['Parada e segurança', false, [
    ['perdido', 1, 2000, 10, 'ms sem ver a linha até desligar'],
    ['tmax', 0, 600, 10, 's até desligar sozinho (0 = sem limite)'],
    ['freio', 0, 300, 5, 'Ré no comando de parada: % do PWM em que vinha'],
    ['tfreio', 0, 1000, 5, 'Duração da ré da parada (ms)'],
  ]],
  ['Controle IR (NEC)', false, [
    ['nec', 0, 1, 1, '1 = aceita o controle NEC do kit'],
    ['necend', 0, 65535, 1, 'Endereço do controle'],
    ['necpart', 0, 255, 1, 'Tecla de partida'],
    ['necpara', 0, 255, 1, 'Tecla de parada'],
  ]],
  ['Outros', false, [
    ['bipe', 0, 120, 5, 'Volume do bipe nos motores (0 = mudo)'],
  ]],
];
const DEF = {};
for (const [, , lista] of GRUPOS) for (const [nome, min, max, passo, txt] of lista) {
  DEF[nome] = { nome, min, max, passo, txt, decimais: Number.isInteger(passo) ? 0 : 3 };
}

// ------------------------------------------------------------------ utilidades
const $ = (s) => document.querySelector(s);
const esperar = (ms) => new Promise((r) => setTimeout(r, ms));
const guardar = (k, v) => { try { localStorage.setItem('pqx.' + k, JSON.stringify(v)); } catch (e) {} };
const ler = (k, padrao) => {
  try { const v = localStorage.getItem('pqx.' + k); return v === null ? padrao : JSON.parse(v); } catch (e) { return padrao; }
};

let toastT = 0;
function avisar(txt, ms = 2600) {
  const t = $('#toast');
  t.textContent = txt;
  t.hidden = false;
  clearTimeout(toastT);
  toastT = setTimeout(() => { t.hidden = true; }, ms);
}

function estado(txt, tipo = '') {
  const e = $('#estado');
  e.textContent = txt;
  e.className = 'estado ' + tipo;
}

// ------------------------------------------------------------------ terminal
const TERM_MAX = 600;
const termLinhas = [];
function termAdd(txt, meu = false) {
  termLinhas.push([txt, meu]);
  if (termLinhas.length > TERM_MAX) termLinhas.splice(0, termLinhas.length - TERM_MAX);
  if (!$('#aba-terminal').classList.contains('ativa')) return;
  termDesenhar();
}
function termDesenhar() {
  const el = $('#terminal');
  const fim = el.scrollTop + el.clientHeight >= el.scrollHeight - 30;
  el.innerHTML = '';
  const frag = document.createDocumentFragment();
  for (const [t, meu] of termLinhas) {
    const d = document.createElement('div');
    if (meu) d.className = 'eu';
    d.textContent = meu ? '> ' + t : t;
    frag.appendChild(d);
  }
  el.appendChild(frag);
  if (fim) el.scrollTop = el.scrollHeight;
}

// ------------------------------------------------------------------ conexao
class Robo {
  constructor() {
    this.disp = null;
    this.rx = null;
    this.resto = '';
    this.ouvintes = new Set();
    this.vez = Promise.resolve();   // um comando de cada vez
    this.escrita = Promise.resolve();
    this.pronto = false;            // conectado e com a senha aceita
    this.dec = new TextDecoder();
    this.enc = new TextEncoder();
  }

  async escolher() {
    this.disp = await navigator.bluetooth.requestDevice({
      // O nome cabe no anuncio; o UUID de 128 bits nao cabe junto
      filters: [{ namePrefix: 'Perseguidor' }, { services: [NUS] }],
      optionalServices: [NUS],
    });
    this.disp.addEventListener('gattserverdisconnected', () => this.caiu());
  }

  async conectar(senha) {
    estado('conectando…', 'meio');
    const gatt = await this.disp.gatt.connect();
    const serv = await gatt.getPrimaryService(NUS);
    this.rx = await serv.getCharacteristic(NUS_RX);
    const tx = await serv.getCharacteristic(NUS_TX);
    tx.addEventListener('characteristicvaluechanged', (ev) => this.chegou(ev.target.value));
    await tx.startNotifications();
    this.resto = '';
    estado('conferindo a senha…', 'meio');
    const r = await this.comandoLivre('senha=' + senha, {
      fim: (l) => /console liberado|mande senha/.test(l), max: 4000,
    });
    if (!r.some((l) => /console liberado/.test(l))) {
      estado('senha recusada', 'erro');
      throw new Error('O robô recusou a senha. Confira na aba Terminal.');
    }
    this.pronto = true;
    estado('conectado: ' + (this.disp.name || 'robô'), 'ok');
  }

  desconectar() {
    if (this.disp && this.disp.gatt.connected) this.disp.gatt.disconnect();
  }

  get conectado() { return !!(this.disp && this.disp.gatt.connected); }

  caiu() {
    this.pronto = false;
    this.rx = null;
    estado('desconectado', 'erro');
    for (const f of [...this.ouvintes]) f(null);
    atualizarBotaoConectar();
  }

  chegou(dv) {
    this.resto += this.dec.decode(dv, { stream: true });
    let i;
    while ((i = this.resto.indexOf('\n')) >= 0) {
      const linha = this.resto.slice(0, i).replace(/\r$/, '');
      this.resto = this.resto.slice(i + 1);
      termAdd(linha);
      for (const f of [...this.ouvintes]) f(linha);
    }
  }

  escrever(txt) {
    const bytes = this.enc.encode(txt);
    this.escrita = this.escrita.then(async () => {
      for (let i = 0; i < bytes.length; i += PEDACO) {
        if (!this.rx) throw new Error('desconectado');
        await this.rx.writeValueWithResponse(bytes.slice(i, i + PEDACO));
      }
    });
    return this.escrita;
  }

  // Manda uma linha e junta as respostas ate 'fim' dar verdadeiro, ou ate
  // 'silencio' ms sem chegar nada (depois da primeira), ou 'max' ms.
  comandoLivre(cmd, { fim = null, silencio = 450, max = 6000, aoLinha = null } = {}) {
    return new Promise((resolve, reject) => {
      const linhas = [];
      let tSil = 0;
      const tMax = setTimeout(() => acabar(), max);
      const acabar = (erro) => {
        clearTimeout(tSil);
        clearTimeout(tMax);
        this.ouvintes.delete(ouvir);
        if (erro) reject(erro); else resolve(linhas);
      };
      const ouvir = (l) => {
        if (l === null) return acabar(new Error('desconectado'));
        linhas.push(l);
        if (aoLinha) aoLinha(l);
        if (fim && fim(l, linhas)) return acabar();
        clearTimeout(tSil);
        if (silencio) tSil = setTimeout(() => acabar(), silencio);
      };
      this.ouvintes.add(ouvir);
      termAdd(cmd, true);
      this.escrever(cmd + '\n').catch(acabar);
    });
  }

  // Na fila: um comando so comeca quando o anterior terminou
  comando(cmd, op) {
    if (!this.pronto) return Promise.reject(new Error('Conecte ao robô primeiro.'));
    const p = this.vez.then(() => this.comandoLivre(cmd, op));
    this.vez = p.catch(() => {});
    return p;
  }

  // Comandos que ficam mandando linhas ate receber qualquer tecla (s, r, ir)
  fluxo(cmd, aoLinha) {
    if (!this.pronto) return Promise.reject(new Error('Conecte ao robô primeiro.'));
    let parar;
    const fim = new Promise((r) => { parar = r; });
    const p = this.vez.then(async () => {
      const ouvir = (l) => { if (l === null) parar(); else aoLinha(l); };
      this.ouvintes.add(ouvir);
      termAdd(cmd, true);
      try {
        await this.escrever(cmd + '\n');
        await fim;
        if (this.conectado) {
          await this.escrever('\n');
          await esperar(250);    // o robo descarta a entrada e volta ao console
        }
      } finally {
        this.ouvintes.delete(ouvir);
      }
    });
    this.vez = p.catch(() => {});
    return { parar: () => parar(), fim: p };
  }
}

const robo = new Robo();

// ------------------------------------------------------------------ parametros (tela)
const valores = {};       // o que o robo confirmou
let naoSalvo = false;
const tEnvio = {};

function montarParametros() {
  const raiz = $('#grupos');
  for (const [titulo, aberto, lista] of GRUPOS) {
    const d = document.createElement('details');
    d.className = 'cartao grupo';
    d.open = aberto;
    d.innerHTML = `<summary>${titulo}</summary>`;
    for (const [nome] of lista) {
      const def = DEF[nome];
      const div = document.createElement('div');
      div.className = 'param';
      div.id = 'p-' + nome;
      div.innerHTML = `
        <div class="param-txt"><b>${nome}</b><span>${def.txt}</span></div>
        <div class="param-ctl">
          <button type="button" data-d="-1" aria-label="diminuir ${nome}">−</button>
          <input type="number" inputmode="decimal" min="${def.min}" max="${def.max}" step="${def.passo}">
          <button type="button" data-d="1" aria-label="aumentar ${nome}">+</button>
        </div>`;
      const inp = div.querySelector('input');
      div.querySelectorAll('button').forEach((b) => b.addEventListener('click', () => {
        const atual = parseFloat(inp.value);
        let v = (isNaN(atual) ? def.min : atual) + Number(b.dataset.d) * def.passo;
        v = Math.min(def.max, Math.max(def.min, v));
        inp.value = formatar(def, v);
        programarEnvio(nome, 500);
      }));
      inp.addEventListener('input', () => { div.className = 'param mudou'; programarEnvio(nome, 1200); });
      inp.addEventListener('change', () => programarEnvio(nome, 0));
      d.appendChild(div);
    }
    raiz.appendChild(d);
  }
}

function formatar(def, v) {
  return def.decimais ? Number(v).toFixed(def.decimais).replace(/0+$/, '').replace(/\.$/, '') : String(Math.round(v));
}

function mostrarValor(nome, v) {
  const div = document.getElementById('p-' + nome);
  if (!div) return;
  const inp = div.querySelector('input');
  if (document.activeElement !== inp) inp.value = formatar(DEF[nome], v);
}

function marcar(nome, cls) {
  const div = document.getElementById('p-' + nome);
  if (div) div.className = 'param ' + cls;
}

function programarEnvio(nome, ms) {
  marcar(nome, 'mudou');
  clearTimeout(tEnvio[nome]);
  tEnvio[nome] = setTimeout(() => enviarParametro(nome), ms);
}

// Le "nome=valor" de qualquer linha (o 'p' lista 5 por linha, separados por tab)
function lerPares(linhas) {
  const achados = {};
  for (const l of linhas) {
    for (const m of l.matchAll(/([a-z]+)=(-?[\d.]+)/g)) {
      if (DEF[m[1]]) achados[m[1]] = parseFloat(m[2]);
    }
  }
  return achados;
}

async function enviarParametro(nome, silencioso = false) {
  const def = DEF[nome];
  const inp = document.querySelector('#p-' + nome + ' input');
  const v = parseFloat(String(inp.value).replace(',', '.'));
  if (isNaN(v) || v < def.min || v > def.max) {
    marcar(nome, 'falhou');
    if (!silencioso) avisar(`${nome}: de ${def.min} a ${def.max}`);
    return false;
  }
  if (valores[nome] !== undefined && Math.abs(valores[nome] - v) < 1e-6) { marcar(nome, ''); return true; }
  try {
    const r = await robo.comando(`${nome}=${formatar(def, v)}`, {
      fim: (l) => l.startsWith(nome + '=') || /desconhecido|fora da faixa/.test(l), max: 2500,
    });
    const lido = lerPares(r)[nome];
    if (lido === undefined) throw new Error(r.join(' ') || 'sem resposta');
    valores[nome] = lido;
    mostrarValor(nome, lido);
    marcar(nome, 'ok');
    marcarNaoSalvo(true);
    if (nome === 'nivel' || nome === 'vel' || nome === 'reta' || nome === 'passo' || nome === 'vent' || nome === 'vpasso') {
      if (!silencioso) lerEstado().catch(() => {});
    }
    return true;
  } catch (e) {
    marcar(nome, 'falhou');
    if (!silencioso) avisar(`${nome}: ${e.message}`);
    return false;
  }
}

function marcarNaoSalvo(sim) {
  naoSalvo = sim;
  $('#btSalvar').classList.toggle('pendente', sim);
  $('#dicaSalvar').innerHTML = sim
    ? '<b>Há mudanças não gravadas.</b> Elas valem até desligar o robô.'
    : 'As mudanças valem na hora, até desligar. <b>Salvar no robô</b> grava de vez.';
}

async function lerParametros() {
  const r = await robo.comando('p', {
    fim: (l, ls) => Object.keys(lerPares(ls)).length >= Object.keys(DEF).length, max: 4000,
  });
  const pares = lerPares(r);
  for (const [k, v] of Object.entries(pares)) { valores[k] = v; mostrarValor(k, v); marcar(k, ''); }
  return pares;
}

async function lerEstado() {
  const r = await robo.comando('?', { fim: (l) => l.startsWith('comandos:'), max: 5000 });
  for (const l of r) {
    let m;
    if ((m = l.match(/^calibracao: (.*?) \| linha (\w+)/))) {
      $('#rCal').textContent = m[1] === 'ok' ? 'ok' : m[1] === 'falta' ? 'falta' : 'gravada (botão 2)';
      $('#rCal').style.color = m[1] === 'ok' ? 'var(--ok)' : 'var(--alerta)';
      $('#rLinha').textContent = m[2] === 'BRANCA' ? 'branca / fundo preto' : 'preta / fundo branco';
    } else if ((m = l.match(/^nivel (\d+): curva (\d+) reta (\d+) EDF (\d+)/))) {
      $('#rNivel').textContent = `${m[1]} (curva ${m[2]}, reta ${m[3]}, EDF ${m[4]})`;
    } else if ((m = l.match(/^leitura da barra: (\d+) us \(max (\d+)\).*ciclo de (\d+)/))) {
      const lento = Number(m[2]) * 10 > Number(m[3]) * 7;
      $('#rLeitura').textContent = `${m[1]} µs (máx ${m[2]}) de ${m[3]}`;
      $('#rLeitura').style.color = lento ? 'var(--perigo)' : '';
    } else if ((m = l.match(/^modulo de inicio: arena (\d+)(.*?) ->/))) {
      $('#inArena').value = m[1];
      $('#rArena').textContent = `Arena gravada: ${m[1]}${m[2] ? ' (padrão, nunca programada)' : ''}.`;
    } else if ((m = l.match(/^(Perseguidor-qx \S+)/))) {
      $('#versao').textContent = 'Firmware: ' + m[1];
    }
  }
}

// ------------------------------------------------------------------ perfis
function perfis() { return ler('perfis', {}); }
function listarPerfis(sel) {
  const s = $('#selPerfil');
  const nomes = Object.keys(perfis()).sort();
  s.innerHTML = nomes.length ? '' : '<option value="">(nenhum perfil)</option>';
  for (const n of nomes) {
    const o = document.createElement('option');
    o.value = o.textContent = n;
    s.appendChild(o);
  }
  if (sel) s.value = sel;
}

// ------------------------------------------------------------------ sensores
let fluxoAtual = null;
function montarBarras() {
  const raiz = $('#barras');
  for (let i = 0; i < 12; i++) {
    const b = document.createElement('div');
    b.className = 'b';
    b.innerHTML = `<i></i><span>S${i}</span>`;
    raiz.appendChild(b);
  }
}
function linhaSensores(l) {
  if (/calibre antes/.test(l)) {
    $('#sAviso').textContent = 'O robô não está calibrado: calibre pelo botão 1 (ou use a gravada com o botão 2).';
    if (fluxoAtual) fluxoAtual.parar();
    return;
  }
  const m = l.match(/^((?:\d+\t){12})erro (-?\d+)\t([L-])([X-])/);
  if (!m) return;
  const v = m[1].trim().split('\t').map(Number);
  const barras = $('#barras').children;
  v.forEach((x, i) => { barras[i].firstChild.style.height = (x / 255 * 100) + '%'; });
  const erro = Number(m[2]);
  $('#sErro').textContent = erro;
  $('#sVista').textContent = m[3] === 'L' ? 'vista' : 'perdida';
  $('#sVista').style.color = m[3] === 'L' ? 'var(--ok)' : 'var(--perigo)';
  $('#sCruz').textContent = m[4] === 'X' ? 'sim' : 'não';
  $('#erroMarca').style.left = (50 + Math.max(-1, Math.min(1, erro / 5500)) * 50) + '%';
}

async function alternarSensores() {
  const bt = $('#btSensores');
  if (fluxoAtual) { fluxoAtual.parar(); return; }
  $('#sAviso').textContent = '';
  fluxoAtual = robo.fluxo('s', linhaSensores);
  bt.textContent = 'Parar';
  try { await fluxoAtual.fim; } catch (e) { avisar(e.message); }
  fluxoAtual = null;
  bt.textContent = 'Começar';
}

// ------------------------------------------------------------------ partida
let logCsv = '';
let logDados = [];

function lerResumo(linhas) {
  const ul = $('#resumo');
  ul.innerHTML = '';
  const traduz = [
    [/^fim: (.*?) \| andou ([\d.]+) s \| linhas de partida (\d+) \(~(\d+) voltas\)/, (m) => [
      `Fim: <b>${m[1].toLowerCase()}</b>`, `Andou <b>${m[2].replace('.', ',')} s</b>`,
      `Linhas de partida: <b>${m[3]}</b> (~${m[4]} voltas)`]],
    [/^curva (\d+) reta (\d+) EDF (\d+) \| em reta (\d+) %/, (m) => [
      `PWM curva <b>${m[1]}</b>, reta <b>${m[2]}</b>, EDF <b>${m[3]}</b>`, `Em reta: <b>${m[4]} %</b> do tempo`]],
    [/^erro max (\d+) \| sem linha (\d+) ms/, (m) => [`Erro máximo: <b>${m[1]}</b>`, `Sem linha: <b>${m[2]} ms</b>`]],
    [/^ciclo max (\d+) us \(periodo (\d+)\), atrasados (\d+)/, (m) => [
      `Ciclo máximo: <b>${m[1]} µs</b> de ${m[2]}`,
      `Ciclos atrasados: <b style="color:${m[3] === '0' ? 'var(--ok)' : 'var(--perigo)'}">${m[3]}</b>`]],
  ];
  let algum = false;
  for (const l of linhas) {
    if (/nenhuma partida/.test(l)) { ul.innerHTML = '<li class="dica">Nenhuma partida desde que o robô ligou.</li>'; return; }
    for (const [re, f] of traduz) {
      const m = l.match(re);
      if (m) for (const t of f(m)) { const li = document.createElement('li'); li.innerHTML = t; ul.appendChild(li); algum = true; }
    }
  }
  if (!algum) ul.innerHTML = '<li class="dica">Sem resposta do robô.</li>';
}

function lerLog(linhas) {
  const i = linhas.findIndex((l) => l.startsWith('ms,erro'));
  if (i < 0) return false;
  const corpo = linhas.slice(i).filter((l) => /^(ms,|-?\d+,)/.test(l));
  logCsv = corpo.join('\n') + '\n';
  logDados = corpo.slice(1).map((l) => l.split(',').map(Number))
    .filter((a) => a.length === 7 && a.every((x) => !isNaN(x)));
  $('#btCsv').disabled = !logDados.length;
  desenharGrafico();
  return true;
}

function cor(nome) { return getComputedStyle(document.documentElement).getPropertyValue(nome).trim(); }

function desenharGrafico() {
  const cv = $('#grafico');
  const dpr = window.devicePixelRatio || 1;
  const L = cv.clientWidth, A = 300;
  cv.width = L * dpr; cv.height = A * dpr;
  const g = cv.getContext('2d');
  g.scale(dpr, dpr);
  g.clearRect(0, 0, L, A);
  g.font = '11px system-ui, sans-serif';
  const fraco = cor('--fraco');
  if (!logDados.length) {
    g.fillStyle = fraco;
    g.fillText('Toque em "Ler registro" depois de uma partida.', 12, A / 2);
    return;
  }
  const esq = 34, dir = 8;
  const pA = { y: 8, h: 120, min: -5500, max: 5500 };     // erro
  const pB = { y: 156, h: 120, min: -255, max: 255 };     // rodas
  const t0 = logDados[0][0], t1 = logDados[logDados.length - 1][0] || 0;
  const x = (t) => esq + (t - t0) / ((t1 - t0) || 1) * (L - esq - dir);
  const y = (p, v) => p.y + (1 - (Math.max(p.min, Math.min(p.max, v)) - p.min) / (p.max - p.min)) * p.h;

  for (const p of [pA, pB]) {
    // trechos sem linha
    g.fillStyle = cor('--c-sem');
    for (let i = 0; i < logDados.length; i++) {
      if (logDados[i][4] === 0) {
        const xa = x(logDados[i][0]), xb = i + 1 < logDados.length ? x(logDados[i + 1][0]) : xa + 2;
        g.fillRect(xa, p.y, Math.max(2, xb - xa), p.h);
      }
    }
    g.strokeStyle = cor('--borda'); g.lineWidth = 1;
    g.strokeRect(esq, p.y, L - esq - dir, p.h);
    g.beginPath(); g.moveTo(esq, y(p, 0)); g.lineTo(L - dir, y(p, 0)); g.stroke();
    g.fillStyle = fraco; g.textAlign = 'right';
    g.fillText(String(p.max), esq - 4, p.y + 9);
    g.fillText('0', esq - 4, y(p, 0) + 4);
    g.fillText(String(p.min), esq - 4, p.y + p.h);
  }
  // faixas atravessadas
  g.strokeStyle = cor('--c-faixa'); g.lineWidth = 2;
  for (const a of logDados) if (a[5]) {
    g.beginPath(); g.moveTo(x(a[0]), pA.y); g.lineTo(x(a[0]), pA.y + 6); g.stroke();
  }
  const serie = (p, col, nomeCor) => {
    g.strokeStyle = cor(nomeCor); g.lineWidth = 1.8; g.beginPath();
    logDados.forEach((a, i) => { const px = x(a[0]), py = y(p, a[col]); if (i) g.lineTo(px, py); else g.moveTo(px, py); });
    g.stroke();
  };
  serie(pA, 1, '--c-erro');
  serie(pB, 2, '--c-esq');
  serie(pB, 3, '--c-dir');
  g.fillStyle = fraco; g.textAlign = 'left';
  g.fillText('erro', esq + 4, pA.y + 12);
  g.fillText('PWM das rodas', esq + 4, pB.y + 12);
  g.textAlign = 'center';
  g.fillText((t0 / 1000).toFixed(1).replace('.', ',') + ' s', x(t0) + 14, A - 4);
  g.fillText('fim', x(t1) - 10, A - 4);
}

// ------------------------------------------------------------------ testes
function saidaTeste(txt, limpar = false) {
  const el = $('#saidaTeste');
  if (limpar) { el.textContent = ''; if (!txt) return; }
  el.textContent += txt + '\n';
  el.scrollTop = el.scrollHeight;
}

async function testeComParar(cmd, aoLinha) {
  if (fluxoAtual) return avisar('Pare o teste em andamento antes.');
  $('#btParar').hidden = false;
  fluxoAtual = robo.fluxo(cmd, aoLinha);
  try { await fluxoAtual.fim; } catch (e) { avisar(e.message); }
  fluxoAtual = null;
  $('#btParar').hidden = true;
}

// ------------------------------------------------------------------ acoes
async function conectar() {
  if (!navigator.bluetooth) { $('#avisoBT').hidden = false; return; }
  if (robo.conectado) { robo.desconectar(); return; }
  try {
    if (!robo.disp) await robo.escolher();
    await robo.conectar($('#inSenha').value.trim());
    atualizarBotaoConectar();
    await lerParametros();
    await lerEstado();
    marcarNaoSalvo(false);
    avisar('Conectado. Parâmetros lidos do robô.');
  } catch (e) {
    if (e.name === 'NotFoundError') { estado('desconectado'); return; }   // cancelou a escolha
    if (robo.conectado && !robo.pronto) robo.desconectar();
    if (!robo.conectado) estado('desconectado', 'erro');
    avisar(e.message, 4000);
  }
  atualizarBotaoConectar();
}

function atualizarBotaoConectar() {
  const b = $('#btConectar');
  b.textContent = robo.conectado ? 'Desconectar' : robo.disp ? 'Reconectar' : 'Conectar';
  b.classList.toggle('primario', !robo.conectado);
}

function confirmar(txt) { return window.confirm(txt); }

const ACOES = {
  async atualizar() { await lerParametros(); await lerEstado(); avisar('Lido do robô.'); },

  async salvar() {
    for (const nome of Object.keys(tEnvio)) clearTimeout(tEnvio[nome]);
    for (const nome of Object.keys(DEF)) {
      const div = document.getElementById('p-' + nome);
      if (div.classList.contains('mudou') || div.classList.contains('falhou')) await enviarParametro(nome, true);
    }
    const r = await robo.comando('salvar', { fim: (l) => /gravados|ERRO/.test(l), max: 3000 });
    if (r.some((l) => /parametros gravados/.test(l))) { marcarNaoSalvo(false); avisar('Gravado no robô.'); }
    else avisar('O robô não confirmou a gravação: ' + (r.join(' ') || 'sem resposta'), 4000);
  },

  async padrao() {
    if (!confirmar('Voltar todos os parâmetros aos valores de fábrica? (Só grava se tocar em Salvar depois.)')) return;
    const r = await robo.comando('padrao', {
      fim: (l, ls) => Object.keys(lerPares(ls)).length >= Object.keys(DEF).length, max: 4000,
    });
    for (const [k, v] of Object.entries(lerPares(r))) { valores[k] = v; mostrarValor(k, v); marcar(k, ''); }
    marcarNaoSalvo(true);
    await lerEstado();
  },

  async arena() {
    const v = Number($('#inArena').value);
    if (!(v >= 0 && v <= 126 && v % 2 === 0)) return avisar('Arena: número par de 0 a 126.');
    await robo.comando('arena=' + v, { fim: (l) => /modulo de inicio|arena:|ERRO/.test(l), silencio: 400, max: 3000 });
    await lerEstado();
    avisar('Arena gravada.');
  },

  'perfil-guardar'() {
    const nome = (window.prompt('Nome do perfil:', $('#selPerfil').value || '') || '').trim();
    if (!nome) return;
    const p = perfis();
    p[nome] = { ...valores };
    guardar('perfis', p);
    listarPerfis(nome);
    avisar(`Perfil "${nome}" guardado no celular.`);
  },

  async 'perfil-carregar'() {
    const nome = $('#selPerfil').value;
    const pf = perfis()[nome];
    if (!pf) return avisar('Escolha um perfil.');
    if (!confirmar(`Mandar o perfil "${nome}" ao robô?`)) return;
    let falhas = 0;
    for (const [k, v] of Object.entries(pf)) {
      if (!DEF[k]) continue;
      document.querySelector('#p-' + k + ' input').value = formatar(DEF[k], v);
      if (!(await enviarParametro(k, true))) falhas++;
    }
    await lerEstado();
    avisar(falhas ? `${falhas} parâmetro(s) não aceitos.` : `Perfil "${nome}" no robô. Toque em Salvar para gravar.`, 3500);
  },

  'perfil-apagar'() {
    const nome = $('#selPerfil').value;
    if (!nome || !confirmar(`Apagar o perfil "${nome}" deste celular?`)) return;
    const p = perfis();
    delete p[nome];
    guardar('perfis', p);
    listarPerfis();
  },

  'perfil-exportar'() {
    const txt = JSON.stringify({ perfis: perfis(), atual: valores }, null, 1);
    baixar('perseguidor-qx-perfis.json', txt, 'application/json');
  },

  sensores: alternarSensores,

  async res() {
    const r = await robo.comando('res', { fim: (l) => /^ciclo max|nenhuma partida/.test(l), max: 4000 });
    lerResumo(r);
  },

  async log() {
    avisar('Lendo o registro (alguns segundos)…');
    const r = await robo.comando('log', { silencio: 900, max: 20000 });
    if (!lerLog(r)) avisar('O robô não mandou o registro.');
  },

  csv() { baixar('perseguidor-qx-log.csv', logCsv, 'text/csv'); },

  async motores() {
    if (!confirmar('As rodas vão girar (0,5 s cada). O robô está suspenso ou seguro?')) return;
    saidaTeste('', true);
    await robo.comando('motores', { fim: (l) => /DIREITA para tras/.test(l), silencio: 0, max: 12000, aoLinha: (l) => saidaTeste(l) });
    await esperar(1300);   // a ultima roda ainda gira depois da mensagem
  },

  async edf() {
    if (!confirmar('A EDF vai ligar por 3 s no nível atual. SW2 ligada, robô seguro e nada perto da entrada?')) return;
    saidaTeste('', true);
    $('#btParar').hidden = false;
    try {
      await robo.comando('edf', { fim: (l) => /EDF desligada/.test(l), silencio: 0, max: 9000, aoLinha: (l) => saidaTeste(l) });
    } finally { $('#btParar').hidden = true; }
  },

  async m() {
    saidaTeste('', true);
    await robo.comando('m', { fim: (l) => /^(OK|NAO CONFERE)/.test(l), silencio: 0, max: 14000, aoLinha: (l) => saidaTeste(l) });
  },

  async ir() { saidaTeste('', true); await testeComParar('ir', (l) => saidaTeste(l)); },
  async r() { saidaTeste('', true); await testeComParar('r', (l) => saidaTeste(l)); },

  async cor() {
    saidaTeste('', true);
    await robo.comando('cor', { fim: (l) => /^linha /.test(l), max: 2500, aoLinha: (l) => saidaTeste(l) });
    await lerEstado();
  },

  async parar() {
    if (fluxoAtual) { fluxoAtual.parar(); return; }
    // 'edf' e 'motores' ocupam a fila: manda a tecla direto
    if (robo.conectado) await robo.escrever('\n');
  },
};

function baixar(nome, txt, tipo) {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([txt], { type: tipo }));
  a.download = nome;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}

// ------------------------------------------------------------------ inicio
function trocarAba(nome) {
  document.querySelectorAll('.aba').forEach((s) => s.classList.toggle('ativa', s.id === 'aba-' + nome));
  document.querySelectorAll('.abas button').forEach((b) => b.classList.toggle('ativa', b.dataset.aba === nome));
  if (nome === 'terminal') termDesenhar();
  if (nome === 'partida') desenharGrafico();
  if (nome !== 'sensores' && fluxoAtual && $('#btSensores').textContent === 'Parar') fluxoAtual.parar();
  guardar('aba', nome);
  window.scrollTo(0, 0);
}

document.addEventListener('DOMContentLoaded', () => {
  montarParametros();
  montarBarras();
  listarPerfis();
  if (!navigator.bluetooth) $('#avisoBT').hidden = false;

  $('#inSenha').value = ler('senha', '1234');
  $('#inSenha').addEventListener('change', () => guardar('senha', $('#inSenha').value.trim()));
  $('#btConectar').addEventListener('click', conectar);

  document.querySelectorAll('.abas button').forEach((b) => b.addEventListener('click', () => trocarAba(b.dataset.aba)));
  document.addEventListener('click', async (ev) => {
    const b = ev.target.closest('[data-acao]');
    if (!b || !ACOES[b.dataset.acao]) return;
    const acao = b.dataset.acao;
    const longa = !['parar', 'sensores', 'ir', 'r'].includes(acao);
    if (longa) b.disabled = true;
    try { await ACOES[acao](); } catch (e) { avisar(e.message, 3500); }
    if (longa) b.disabled = false;
  });

  $('#formTerminal').addEventListener('submit', async (ev) => {
    ev.preventDefault();
    const cmd = $('#inTerminal').value.trim();
    if (!cmd) return;
    $('#inTerminal').value = '';
    try {
      if (!robo.pronto) throw new Error('Conecte ao robô primeiro.');
      if (['s', 'r', 'ir'].includes(cmd.toLowerCase())) throw new Error('Esse comando não para sozinho: use as abas Sensores e Testes.');
      await robo.comando(cmd, { silencio: 700, max: 15000 });
    } catch (e) { avisar(e.message); }
  });

  window.addEventListener('resize', () => { if ($('#aba-partida').classList.contains('ativa')) desenharGrafico(); });
  trocarAba(ler('aba', 'ajustes'));

  if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});
});
