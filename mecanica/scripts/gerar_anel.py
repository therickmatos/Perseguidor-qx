"""Anel de fixação (braçadeira com aba) da EDF QX-Motor 30 mm — versão final para PLA.

Referências (z = 0 na face de cima da placa, onde a aba encosta):
- Corpo do duto Ø34,0 mm; boca alargada Ø37,5 mm e 5 mm de altura (folha de dados = medido).
- Fenolite 1,5 mm; motores por cima da placa; rodas Ø22 mm -> face de cima da placa a ~6,0 mm do chão.
- Entrada da turbina a ~2 mm do chão -> topo da boca ~1 mm acima da placa (dentro do rebaixo de 3 mm).
- Parafusos: 3x M3x8 nas abas (cabeça por baixo da placa, porca parlock em cima da aba);
  1x M3x12 de aperto com porca presa no rebaixo sextavado.
- Furos da placa para as abas com 4,0 mm: a braçadeira, ao fechar, desloca as abas até ~0,5 mm (A) / ~0,8 mm (B).
- Ângulos: 0° = direita (+x), 90° = frente (+y, lado da barra e da bateria), 270° = trás.

Saídas: anel A (furo 34,3), anel B (furo 34,5), o anel da figura (furo 34,4, sem
entalhe; idêntico ao STL de 17/11/2026), uma placa de impressão com A e B,
contorno 2D e o DXF dos furos da placa. Vão para mecanica/stl/ (ou para a pasta
dada na linha de comando: python3 gerar_anel.py <pasta>).
"""
import json, math, os, sys
import numpy as np
import trimesh
from trimesh.creation import cylinder, box
from shapely.geometry import Point, box as sbox
from shapely.ops import unary_union
from shapely import affinity

BASE = dict(
    duto=34.0, folga_furo=0.3,          # A: 34,3 | B: 34,5
    parede=2.4,                         # parede da zona de aperto
    boca=37.5, folga_boca=1.1,          # rebaixo Ø38,6 para a boca alargada
    parede_rebaixo=1.6,
    prof_rebaixo=3.0,                   # rebaixo até z = 3,0 + chanfro 45° (entrada guiada do duto)
    altura=14.0,
    aba_esp=2.5, aba_r=22.0,            # aba circular de apoio
    abas_ang=[210, 270, 330],           # orelhas de fixação fora da frente (bateria)
    aba_R=24.5, aba_raio=3.5, furo_aba=3.4,
    orelha_ang=300, orelha_esp=5.0, fenda=2.0, orelha_r=28.0,
    aperto_r=23.5, aperto_z=9.5, furo_aperto=3.5,
    porca_af=5.8, porca_prof=2.8,       # porca M3 (5,5 entre faces, 2,4 de espessura) + folga para PLA
    furo_placa=38.0, furo_placa_m3=4.0,
    marcas=1,
)
VARIANTES = {'A': dict(folga_furo=0.3, marcas=1), 'B': dict(folga_furo=0.5, marcas=2),
             'figura': dict(folga_furo=0.4, marcas=0)}   # a da figura: furo 34,4, sem entalhe
NA_PLACA = ('A', 'B')                                       # vão juntos na placa de impressão

def nome_arquivo(nome, p):
    letra = '' if nome == 'figura' else nome + '_'
    return f'anel_edf_qx30_{letra}furo{p["duto"] + p["folga_furo"]:.1f}.stl'

def rot_z(m, deg):
    m.apply_transform(trimesh.transformations.rotation_matrix(math.radians(deg), (0, 0, 1)))
    return m

def revolve(profile):
    return trimesh.creation.revolve(np.array(profile, dtype=float), sections=192)

def geom(p):
    r_furo = (p['duto'] + p['folga_furo']) / 2
    r_reb = (p['boca'] + p['folga_boca']) / 2
    g = dict(r_furo=r_furo, r_ap_ext=r_furo + p['parede'], r_reb=r_reb,
             r_reb_ext=r_reb + p['parede_rebaixo'], z_reb=p['prof_rebaixo'])
    g['z_ch'] = g['z_reb'] + (r_reb - r_furo)
    g['larg'] = 2 * p['orelha_esp'] + p['fenda']
    return g

def build(p):
    g = geom(p); H = p['altura']
    ext = revolve([(0, 0), (p['aba_r'], 0), (p['aba_r'], p['aba_esp']), (g['r_reb_ext'], p['aba_esp']),
                   (g['r_reb_ext'], g['z_ch']), (g['r_ap_ext'], g['z_ch']), (g['r_ap_ext'], H), (0, H)])
    pecas = [ext]
    for a in p['abas_ang']:
        c1 = cylinder(radius=p['aba_raio'], height=p['aba_esp'], sections=64); c1.apply_translation((p['aba_R'], 0, p['aba_esp'] / 2))
        c2 = cylinder(radius=p['aba_raio'], height=p['aba_esp'], sections=64); c2.apply_translation((g['r_reb_ext'] - 1, 0, p['aba_esp'] / 2))
        pecas.append(rot_z(trimesh.convex.convex_hull(trimesh.util.concatenate([c1, c2])), a))
    o = box(extents=(p['orelha_r'] - (g['r_ap_ext'] - 0.8), g['larg'], H))      # orelhas apoiadas na mesa
    o.apply_translation(((p['orelha_r'] + g['r_ap_ext'] - 0.8) / 2, 0, H / 2))
    pecas.append(rot_z(o, p['orelha_ang']))
    solido = trimesh.boolean.union(pecas)

    cortes = [revolve([(0, -1), (g['r_reb'], -1), (g['r_reb'], g['z_reb']), (g['r_furo'], g['z_ch']),
                       (g['r_furo'], H + 1), (0, H + 1)])]
    f = box(extents=(p['orelha_r'] + 5 - (g['r_furo'] - 1), p['fenda'], H + 4))
    f.apply_translation(((p['orelha_r'] + 5 + g['r_furo'] - 1) / 2, 0, H / 2)); cortes.append(rot_z(f, p['orelha_ang']))
    pa = cylinder(radius=p['furo_aperto'] / 2, height=g['larg'] + 4, sections=48)
    pa.apply_transform(trimesh.transformations.rotation_matrix(math.pi / 2, (1, 0, 0)))
    pa.apply_translation((p['aperto_r'], 0, p['aperto_z'])); cortes.append(rot_z(pa, p['orelha_ang']))
    hexa = cylinder(radius=p['porca_af'] / math.sqrt(3), height=p['porca_prof'] + 1, sections=6)
    hexa.apply_transform(trimesh.transformations.rotation_matrix(math.pi / 2, (1, 0, 0)))
    hexa.apply_transform(trimesh.transformations.rotation_matrix(math.pi / 2, (0, 1, 0)))   # vértice para cima
    hexa.apply_translation((p['aperto_r'], g['larg'] / 2 - p['porca_prof'] + (p['porca_prof'] + 1) / 2, p['aperto_z']))
    cortes.append(rot_z(hexa, p['orelha_ang']))
    for a in p['abas_ang']:
        h = cylinder(radius=p['furo_aba'] / 2, height=p['aba_esp'] + 2, sections=48)
        h.apply_translation((p['aba_R'], 0, p['aba_esp'] / 2)); cortes.append(rot_z(h, a))
    for k in range(p['marcas']):                                                  # entalhes de identificação (A=1, B=2)
        m = box(extents=(1.2, 1.4, 1.0)); m.apply_translation((24.0 + 2.2 * k, -(p['fenda'] / 2 + p['orelha_esp'] / 2), H))
        cortes.append(rot_z(m, p['orelha_ang']))
    return trimesh.boolean.difference([solido] + cortes), g

def contorno_2d(p, g):
    formas = [Point(0, 0).buffer(p['aba_r'], 128), Point(0, 0).buffer(g['r_reb_ext'], 128)]
    for a in p['abas_ang']:
        t = unary_union([Point(p['aba_R'], 0).buffer(p['aba_raio'], 64),
                         Point(g['r_reb_ext'] - 1, 0).buffer(p['aba_raio'], 64)]).convex_hull
        formas.append(affinity.rotate(t, a, origin=(0, 0)))
    formas.append(affinity.rotate(sbox(g['r_ap_ext'] - 0.8, -g['larg'] / 2, p['orelha_r'], g['larg'] / 2),
                                  p['orelha_ang'], origin=(0, 0)))
    return unary_union(formas)

if __name__ == '__main__':
    # padrao: mecanica/stl/ (ao lado desta pasta)
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'stl')
    info, placa = {}, []
    for nome, var in VARIANTES.items():
        p = dict(BASE, **var)
        anel, g = build(p)
        assert anel.is_watertight and len(anel.split(only_watertight=False)) == 1
        anel.export(f'{out}/{nome_arquivo(nome, p)}')
        info[nome] = dict(furo=round(2 * g['r_furo'], 2), massa_pla_g=round(anel.volume / 1000 * 1.24, 1),
                          limites=np.round(anel.bounds, 2).tolist())
        if nome in NA_PLACA:
            c = anel.copy(); c.apply_translation((-32 if nome == 'A' else 32, 0, 0)); placa.append(c)
        if nome == 'A':
            p_ref, g_ref = p, g
    trimesh.util.concatenate(placa).export(f'{out}/placa_impressao_anel_A_e_B.stl')
    c = contorno_2d(p_ref, g_ref)
    furos = [(round(p_ref['aba_R'] * math.cos(math.radians(a)), 3), round(p_ref['aba_R'] * math.sin(math.radians(a)), 3))
             for a in p_ref['abas_ang']]
    json.dump(dict(contorno=list(c.exterior.coords), furos_m3=furos), open(f'{out}/anel_2d.json', 'w'))
    try:
        import ezdxf
        doc = ezdxf.new('R2010'); doc.units = ezdxf.units.MM; doc.header['$INSUNITS'] = 4
        for n, col in (('TURBINA_FURO_38', 5), ('ANEL_FUROS_M3', 1), ('ANEL_CONTORNO', 3)): doc.layers.add(n, color=col)
        msp = doc.modelspace()
        msp.add_circle((0, 0), p_ref['furo_placa'] / 2, dxfattribs={'layer': 'TURBINA_FURO_38'})
        for x, y in furos: msp.add_circle((x, y), p_ref['furo_placa_m3'] / 2, dxfattribs={'layer': 'ANEL_FUROS_M3'})
        msp.add_lwpolyline(list(c.exterior.coords)[:-1], close=True, dxfattribs={'layer': 'ANEL_CONTORNO'})
        doc.saveas(f'{out}/anel_turbina_placa.dxf')
    except ImportError:
        pass
    print(json.dumps(info, indent=1))
