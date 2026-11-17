"""Netlist de um .esch do EasyEDA Pro: pinos dos componentes x fios com NET x netports.

Uso: python3 -I netlist_epro.py <pasta do .epro extraido> <arquivo .esch>
(o .epro e um zip: extrair antes, por exemplo com unzip -d pasta arquivo.epro)
"""
import json, math, sys, os
from collections import defaultdict

def load(p): return [json.loads(l) for l in open(p, encoding='utf-8') if l.strip()]

def rot(x, y, deg):
    a = math.radians(deg); c, s = round(math.cos(a)), round(math.sin(a))
    return x*c - y*s, x*s + y*c

def netlist(root, sheet):
    rows = load(sheet)
    proj = json.load(open(os.path.join(root, 'project.json'), encoding='utf-8'))
    comps, attrs = {}, defaultdict(dict)
    wires = {}
    for r in rows:
        if r[0] == 'COMPONENT': comps[r[1]] = dict(x=r[3], y=r[4], rot=r[5], mir=r[6])
        elif r[0] == 'WIRE': wires[r[1]] = r[2]
        elif r[0] == 'ATTR': attrs[r[2]][r[3]] = r[4]
    pins = {}   # (x,y) -> list of (designator, pinnumber, pinname)
    ports = {}  # (x,y) -> netname from netport
    for cid, c in comps.items():
        dev = attrs[cid].get('Device')
        if not dev or dev not in proj['devices']: continue
        sym = attrs[cid].get('Symbol') or proj['devices'][dev]['attributes'].get('Symbol')
        title = proj['devices'][dev]['title']
        srows = load(os.path.join(root, 'SYMBOL', sym + '.esym'))
        sp, sa = {}, defaultdict(dict)
        for r in srows:
            if r[0] == 'PIN': sp[r[1]] = (r[4], r[5])
            elif r[0] == 'ATTR': sa[r[2]][r[3]] = r[4]
        for pid, (px, py) in sp.items():
            if c['mir']: px = -px
            dx, dy = rot(px, py, c['rot'])
            pt = (round(c['x'] + dx, 3), round(c['y'] + dy, 3))
            if title.startswith('Netport'):
                ports.setdefault(pt, []).append(attrs[cid].get('Name'))
            elif title.startswith('Drawing'):
                pass
            else:
                pins.setdefault(pt, []).append((attrs[cid].get('Designator'), sa[pid].get('NUMBER'), sa[pid].get('NAME')))
    # fios: pontos e nome
    segs = []
    for wid, polys in wires.items():
        name = attrs[wid].get('NET')
        for poly in polys:
            pts = [(round(poly[i],3), round(poly[i+1],3)) for i in range(0, len(poly), 2)]
            segs.append((wid, name, pts))
    # union-find sobre pontos
    parent = {}
    def f(a):
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a
    def u(a, b): parent[f(a)] = f(b)
    allpts = set(pins) | set(ports)
    for _, _, pts in segs: allpts |= set(pts)
    def on_seg(p, a, b):
        (x, y), (x1, y1), (x2, y2) = p, a, b
        if abs((x2-x1)*(y-y1) - (y2-y1)*(x-x1)) > 1e-6: return False
        return min(x1,x2)-1e-6 <= x <= max(x1,x2)+1e-6 and min(y1,y2)-1e-6 <= y <= max(y1,y2)+1e-6
    names = defaultdict(set)
    for wid, name, pts in segs:
        for i in range(len(pts)):
            u(pts[i], pts[0])
            if i:
                for p in allpts:
                    if on_seg(p, pts[i-1], pts[i]): u(p, pts[0])
        if name: names[pts[0]].add(name)
    for p, nm in ports.items():
        for n in nm:
            if n: names[p].add(n)
    for p in allpts: f(p)
    groups = defaultdict(lambda: dict(names=set(), pins=[]))
    for p in allpts:
        g = groups[f(p)]
        for q in [p]:
            g['names'] |= names.get(q, set())
            g['pins'] += pins.get(q, [])
    # nomes de grupos -> rede (redes de mesmo nome se juntam)
    nets = defaultdict(set)
    anon = 0
    for k, g in groups.items():
        if not g['pins']: continue
        if g['names']:
            for n in g['names']: nets[n] |= set(g['pins'])
            if len(g['names']) > 1: print('AVISO: grupo com varios nomes', g['names'], file=sys.stderr)
        else:
            anon += 1; nets['<sem nome %d>' % anon] |= set(g['pins'])
    return {n: sorted(v, key=lambda t: (str(t[0]), str(t[1]))) for n, v in nets.items()}, pins

if __name__ == '__main__':
    nets, pins = netlist(sys.argv[1], sys.argv[2])
    for n in sorted(nets):
        print(f'{n:12s}', ' '.join(f'{d}.{num}({nm})' for d, num, nm in nets[n]))
    used = set(p for v in nets.values() for p in v)
    allp = set(p for v in pins.values() for p in v)
    print('pinos sem ligacao:', sorted(allp - used, key=str))
