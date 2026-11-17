"""Confere a netlist do esquematico com as redes dos pads do PCB num .epro extraido.

Uso: python3 -I confere_pcb.py <pasta do .epro extraido>
Compara pino a pino (pelo Unique ID do componente). Tem de terminar com "diferencas: 0".
"""
import json, glob, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from netlist_epro import netlist
raiz = sys.argv[1]
sch = glob.glob(raiz + '/SHEET/*/*.esch')[0]
nets, _ = netlist(raiz, sch)
pino_rede = {}
for n, ps in nets.items():
    for d, num, nm in ps:
        pino_rede[(d, num)] = None if n.startswith('<sem nome') else n
rows = [json.loads(l) for l in open(sch, encoding='utf-8')]
uid_des = {}
attrs = {}
for r in rows:
    if r[0] == 'ATTR': attrs.setdefault(r[2], {})[r[3]] = r[4]
for cid, a in attrs.items():
    if 'Unique ID' in a and 'Designator' in a: uid_des[a['Unique ID']] = a['Designator']
pcb = [json.loads(l) for l in open(glob.glob(raiz + '/PCB/*.epcb')[0], encoding='utf-8')]
comp_uid = {r[1]: r[7].get('Unique ID') for r in pcb if r[0] == 'COMPONENT'}
dif = 0; n = 0
for r in pcb:
    if r[0] == 'PAD_NET' and r[2]:
        d = uid_des.get(comp_uid.get(r[1]))
        esq = pino_rede.get((d, r[2]))
        pc = r[3] or None
        n += 1
        if esq != pc:
            dif += 1; print('diferente:', d, 'pino', r[2], 'esquematico', esq, 'PCB', pc)
print('pads conferidos:', n, 'diferencas:', dif)
