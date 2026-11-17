"""Troca os Netport-IN/OUT do esquematico por fios curtos com rotulo de rede (como o da bateria).

Uso: python3 -I netports_para_rotulos.py <pasta do epro extraido>
Mexe so no .esch. Cada netport tem, no mesmo ponto do pino, um fio de comprimento
zero com o atributo NET escondido: esse fio vira um toco que sai do pino na
direcao em que o netport saia, e o NET fica visivel em cima dele.
"""
import json, math, os, sys, glob
from collections import defaultdict

PASSO_CHAR = 5.5      # largura estimada de um caractere na fonte 8 (unidades do esquematico)
COMP_MIN = 30         # comprimento minimo do toco
RENOMEAR = {'BAT': 'VBAT'}   # rotulo da bateria: no PCB o pad 1 do XT60 ja e VBAT

def carregar(p):
    return [json.loads(l) for l in open(p, encoding='utf-8') if l.strip()]

def girar(x, y, graus):
    a = math.radians(graus); c, s = round(math.cos(a)), round(math.sin(a))
    return x * c - y * s, x * s + y * c

def main(raiz):
    proj = json.load(open(os.path.join(raiz, 'project.json'), encoding='utf-8'))
    netport_in = {k for k, v in proj['devices'].items() if v['title'] == 'Netport-IN'}
    netport_out = {k for k, v in proj['devices'].items() if v['title'] == 'Netport-OUT'}
    (folha,) = glob.glob(os.path.join(raiz, 'SHEET', '*', '*.esch'))
    linhas = carregar(folha)

    comps = {r[1]: r for r in linhas if r[0] == 'COMPONENT'}
    attrs = defaultdict(dict)
    for r in linhas:
        if r[0] == 'ATTR':
            attrs[r[2]][r[3]] = r
    estilos = {r[1] for r in linhas if r[0] == 'FONTSTYLE'}

    # netports: ponto de ligacao, direcao para fora e nome
    portas = {}
    for cid, c in comps.items():
        dev = attrs[cid].get('Device', [None] * 5)[4]
        if dev in netport_in or dev in netport_out:
            base = (-1, 0) if dev in netport_in else (1, 0)
            dx, dy = girar(*base, c[5])
            if c[6]:
                dx = -dx
            portas[cid] = dict(p=(c[3], c[4]), d=(dx, dy), nome=attrs[cid]['Name'][4])
    # fios de comprimento zero com NET no mesmo ponto
    fio_no_ponto = {}
    for r in linhas:
        if r[0] == 'WIRE' and len(r[2]) == 1 and len(r[2][0]) == 4:
            x1, y1, x2, y2 = r[2][0]
            if (x1, y1) == (x2, y2):
                fio_no_ponto[(x1, y1)] = r
    # comprimento do toco: o mesmo para os pinos de um lado de um componente,
    # o bastante para o maior nome caber em cima do fio
    grupos = defaultdict(list)
    for cid, pt in portas.items():
        chave = (pt['d'], pt['p'][0] if pt['d'][0] == 0 else None, pt['p'][1] if pt['d'][1] == 0 else None)
        # agrupa por direcao e pela coordenada fixa da coluna/linha de pinos
        chave = (pt['d'], pt['p'][1] if pt['d'][0] == 0 else pt['p'][0])
        grupos[chave].append(cid)
    comp = {}
    for chave, ids in grupos.items():
        maior = max(len(portas[i]['nome']) for i in ids)
        L = max(COMP_MIN, int(math.ceil((10 + PASSO_CHAR * maior + 5) / 10.0)) * 10)
        for i in ids:
            comp[i] = L

    novo_estilo = None
    for k in range(10, 100):
        if 'st%d' % k not in estilos:
            novo_estilo = 'st%d' % k
            break

    remover = set()
    for cid in portas:
        remover.add(cid)
    trocar_fio = {}
    trocar_attr = {}
    for cid, pt in portas.items():
        fio = fio_no_ponto.get(tuple(pt['p']))
        if fio is None:
            sys.exit('netport %s sem fio de comprimento zero no ponto %s' % (cid, pt['p']))
        x, y = pt['p']
        dx, dy = pt['d']
        L = comp[cid]
        trocar_fio[fio[1]] = [x, y, x + dx * L, y + dy * L]
        # rotulo em cima do fio, comecando a 10 do pino e crescendo para fora
        if dy == 0:
            rot = 0
            estilo = 'st1' if dx > 0 else novo_estilo   # st1: o da bateria (alinhado a esquerda)
        else:
            rot = 90
            # texto girado 90: cresce para cima; para baixo, alinhado a direita
            estilo = novo_estilo if dy < 0 else 'st1'
        trocar_attr[fio[1]] = dict(x=x + dx * 10, y=y + dy * 10, rot=rot, estilo=estilo, nome=pt['nome'])

    saida = []
    for r in linhas:
        if r[0] == 'COMPONENT' and r[1] in remover:
            continue
        if r[0] == 'ATTR' and r[2] in remover:
            continue
        if r[0] == 'WIRE' and r[1] in trocar_fio:
            r = r[:2] + [[trocar_fio[r[1]]]] + r[3:]
        if r[0] == 'ATTR' and r[3] == 'NET' and r[2] in trocar_attr:
            t = trocar_attr[r[2]]
            if r[4] != t['nome']:
                sys.exit('nome diferente no fio %s: %s x %s' % (r[2], r[4], t['nome']))
            r = [r[0], r[1], r[2], 'NET', r[4], 0, 1, t['x'], t['y'], t['rot'], t['estilo'], r[11]]
        if r[0] == 'ATTR' and r[3] == 'NET' and r[4] in RENOMEAR:
            r = r[:4] + [RENOMEAR[r[4]]] + r[5:]
        saida.append(r)
        if r[0] == 'FONTSTYLE' and r[1] == 'st1':
            # mesmo estilo do rotulo da bateria, alinhado a direita
            saida.append(['FONTSTYLE', novo_estilo] + r[2:10] + [r[10], 2])

    with open(folha, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(json.dumps(r, ensure_ascii=False, separators=(',', ':')) for r in saida))
    print('netports trocados: %d | estilo novo: %s | comprimentos: %s' %
          (len(portas), novo_estilo, sorted(set(comp.values()))))

if __name__ == '__main__':
    main(sys.argv[1])
