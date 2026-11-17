"""
Braços da barra de sensores — seguidor de linha Galileia (placa no contorno do El Barto).

Gera um STL com os dois braços (esquerdo e direito) deitados para impressão
e um STL de referência com os braços montados, em mecanica/stl/. Tudo em milímetros.

Medidas de entrada:
  - placa base: furos da frente a 45,98 mm um do outro, parafuso M2,5×8;
  - barra (Gerber V1_Barra_de_sensores, 2026-03): furos Ø2,05 a 89,25 mm, parafuso M2.
    Os 12 QRE1113GR ficam num arco à frente dos furos: os do centro a 10,6 mm,
    os das pontas a 5,6 mm. Em cima da barra há um componente em |x| ≈ 40,5 mm,
    por isso o trecho reto do braço fica todo em |x| >= 42,1 mm.

Coordenadas de um braço montado:
  origem = centro do furo da frente da placa (um de cada lado do conector da barra),
  x para dentro do robô no braço esquerdo, y para a frente, z para cima,
  z = 0 na face de cima da placa.

Como funciona:
  - pé: apoia na placa em volta do furo da frente. O furo do pé é oblongo (±0,6 mm
    na frente/trás) e um batente por baixo encosta na borda da frente da placa.
    O batente impede o braço de girar em volta do parafuso;
  - braço: sai do pé, abre para fora e segue reto para a frente na linha dos
    furos da barra;
  - furos para a barra na linha dos furos dela, com números em cm a cada 10 mm
    na lateral de fora. Duas versões (VERSOES):
      'longo': furos a cada 5 mm, de 50 a 185 mm. A ponta do braço e a frente da
               barra (no furo de 185) ficam dentro de 200 mm do furo da placa;
      '12cm':  120 mm do fundo do pé até a ponta, furos a cada 10 mm, de 50 a 110 mm.

Rodar:  python3 gerar_bracos.py          (as duas versões)
        python3 gerar_bracos.py 12cm     (só uma)
Precisa de numpy, shapely, trimesh, manifold3d, matplotlib.
"""
import os
import numpy as np
import trimesh
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union
from shapely import affinity
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties

# ------------------------------------------------------------------ parâmetros
A = 45.98 / 2      # metade da distância entre os furos da frente da placa
B = 89.252 / 2     # metade da distância entre os furos da barra (Gerber)
BORDA = 3.8        # do centro do furo da placa até a borda da frente (medido: 3,6 ± 0,3)
LIMITE = 200.0     # nada passa de 200 mm a partir do furo da placa
BARRA_FRENTE = 13.57             # frente da barra (no centro) à frente dos furos dela

ESP = 4.0          # espessura do braço
LARG = 9.0         # largura do trecho reto
X_INTERNO = 42.1   # o trecho reto fica em |x| >= 42,1 (componente da barra em |x| ≈ 40,5)
LARG_DIAG = 8.5    # largura do trecho diagonal
FURO_BARRA = 2.4   # furo para M2 (a impressora fecha ~0,1–0,2 mm)
FURO_PLACA = 2.9   # furo para M2,5
FOLGA_Y = 0.6      # o furo do pé corre ±0,6 mm para o batente encostar na borda
LABIO_ESP = 1.6    # espessura do batente (sentido da frente)
LABIO_ALT = 1.3    # quanto o batente desce abaixo do braço (placa de 1,5 mm)
LABIO_X = (-1.0, 8.5)        # extensão do batente a partir do furo (+ = para dentro)
PE_X = (-5.5, 10.0)          # pé: de 5,5 mm para fora até 10 mm para dentro do furo
PE_TRAS = -3.8               # pé: até 3,8 mm atrás do centro do furo
TEXTO_ALT = 2.8              # números (cm) na lateral de fora
TEXTO_PROF = 0.4

XS = -(B - A)                         # linha dos furos da barra no braço esquerdo
X_DENTRO = -(X_INTERNO - A)           # borda de dentro do trecho reto
X_FORA = X_DENTRO - LARG              # borda de fora do trecho reto

# ------------------------------------------------------------------ versões
# furos: do furo da placa ao furo da barra (mm), no sentido da frente
# comprimento: do fundo do pé até a ponta do braço (None = ponta 4 mm depois do último furo)
VERSOES = {
    'longo': dict(furos=list(range(50, 186, 5)), comprimento=None,
                  arquivo='bracos_barra_M2.5_M2.stl'),
    '12cm': dict(furos=list(range(50, 111, 10)), comprimento=120.0,
                 arquivo='bracos_barra_12cm_M2.5_M2.stl'),
}


def usar(nome):
    """Escolhe a versão: furos (DIST), ponta do braço (FIM) e nome do arquivo."""
    global VERSAO, DIST, FIM, ARQUIVO, REFERENCIA
    v = VERSOES[nome]
    VERSAO, DIST, ARQUIVO = nome, v['furos'], v['arquivo']
    REFERENCIA = 'bracos_montados_referencia%s.stl' % ('' if nome == 'longo' else '_' + nome)
    FIM = max(DIST) + 4.0 if v['comprimento'] is None else PE_TRAS + v['comprimento']
    assert max(DIST) + FURO_BARRA / 2 + 2.0 <= FIM, 'pouco material depois do último furo'
    assert FIM <= LIMITE and max(DIST) + BARRA_FRENTE <= LIMITE


usar('longo')


def braco_2d():
    """Contorno do braço esquerdo visto de cima (com furos) e o retângulo do pé."""
    joelho_y = min(DIST) - 12.0
    xc = (X_DENTRO + X_FORA) / 2
    inicio = (0, PE_TRAS + LARG_DIAG / 2)
    diag = LineString([inicio, (0, BORDA + 2.0), (xc, joelho_y)]).buffer(LARG_DIAG / 2, quad_segs=24)
    reto = box(X_FORA + 2, joelho_y - LARG_DIAG / 2 + 2, X_DENTRO - 2, FIM - 2).buffer(2.0, quad_segs=12)
    pe = box(PE_X[0] + 2, PE_TRAS + 2, PE_X[1] - 2, BORDA + LABIO_ESP - 2).buffer(2.0, quad_segs=12)
    contorno = unary_union([diag, reto, pe])
    furo_pe = LineString([(0, -FOLGA_Y), (0, FOLGA_Y)]).buffer(FURO_PLACA / 2, quad_segs=24)
    furos = [Point(XS, d).buffer(FURO_BARRA / 2, quad_segs=24) for d in DIST]
    return contorno.difference(unary_union([furo_pe] + furos)), pe


def labio_2d(pe):
    return box(LABIO_X[0], BORDA, LABIO_X[1], BORDA + LABIO_ESP).intersection(pe)


def extrudar(poly, altura, z0=0.0):
    partes = list(poly.geoms) if hasattr(poly, 'geoms') else [poly]
    m = trimesh.util.concatenate([trimesh.creation.extrude_polygon(p, altura) for p in partes])
    m.apply_translation([0, 0, z0])
    return m


def texto_2d(s, altura):
    tp = TextPath((0, 0), s, size=altura, prop=FontProperties(family='DejaVu Sans', weight='bold'))
    aneis = [Polygon(p).buffer(0) for p in tp.to_polygons() if len(p) >= 3]
    aneis = [a for a in aneis if a.area > 1e-4]
    aneis.sort(key=lambda a: -a.area)
    g = None
    for a in aneis:  # par-ímpar: anel dentro de um número ímpar de outros = furo da letra
        dentro = sum(1 for b in aneis if b is not a and b.area > a.area and b.contains(a.representative_point()))
        g = a if g is None else (g.difference(a) if dentro % 2 else g.union(a))
    minx, miny, maxx, maxy = g.bounds
    k = altura / (maxy - miny)
    return affinity.scale(affinity.translate(g, -(minx + maxx) / 2, -(miny + maxy) / 2), k, k, origin=(0, 0))


def numeros(lado):
    """Números em cm a cada 10 mm na lateral de fora, legíveis para quem olha de fora.
    lado = -1 braço esquerdo (face em x = X_FORA), +1 direito (face em x = -X_FORA)."""
    u = np.array([0, lado * 1.0, 0])      # sentido de leitura
    v = np.array([0, 0, 1.0])             # para cima
    w = np.array([lado * 1.0, 0, 0])      # normal para fora (u × v = w: sem espelhar o texto)
    x_face = X_FORA if lado < 0 else -X_FORA
    cortes = []
    for d in DIST:
        if d % 10:
            continue
        m = extrudar(texto_2d(str(d // 10), TEXTO_ALT), TEXTO_PROF + 0.3)
        T = np.eye(4); T[:3, 0], T[:3, 1], T[:3, 2] = u, v, w
        T[:3, 3] = [x_face - w[0] * TEXTO_PROF, d, ESP / 2]
        m.apply_transform(T)
        cortes.append(m)
    return trimesh.boolean.union(cortes, engine='manifold')


def braco_3d(lado):
    """lado = -1 esquerdo, +1 direito (espelho do esquerdo; números refeitos para ler de fora)."""
    contorno, pe = braco_2d()
    corpo = extrudar(contorno, ESP)
    corpo = corpo.union(extrudar(labio_2d(pe), LABIO_ALT + 0.01, -LABIO_ALT), engine='manifold')
    if lado > 0:
        corpo.apply_transform(np.diag([-1.0, 1, 1, 1]))   # trimesh corrige as faces ao espelhar
    return corpo.difference(numeros(lado), engine='manifold')


def para_impressao(m):
    """Face de cima do braço na mesa, batente para cima: imprime sem suporte."""
    m = m.copy()
    m.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [0, 1, 0]))
    m.apply_translation([0, 0, -m.bounds[0][2]])
    return m


if __name__ == '__main__':
    import sys
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'stl')   # mecanica/stl/
    for nome in (sys.argv[1:] or list(VERSOES)):
        usar(nome)
        esq, dir_ = braco_3d(-1), braco_3d(+1)
        for lado, m in (('esquerdo', esq), ('direito', dir_)):
            assert m.is_watertight and m.is_volume, lado
        montado = trimesh.util.concatenate([esq.copy().apply_translation([-A, 0, 0]),
                                            dir_.copy().apply_translation([A, 0, 0])])
        montado.export(os.path.join(out, REFERENCIA))
        pe_, pd_ = para_impressao(esq), para_impressao(dir_)
        pe_.apply_translation([-pe_.bounds[1][0] - 3.0, -pe_.bounds[0][1], 0])
        pd_.apply_translation([-pd_.bounds[0][0] + 3.0, -pd_.bounds[0][1], 0])
        placa = trimesh.util.concatenate([pe_, pd_])
        placa.apply_translation([-(placa.bounds[0][0] + placa.bounds[1][0]) / 2,
                                 -(placa.bounds[0][1] + placa.bounds[1][1]) / 2, 0])
        placa.export(os.path.join(out, ARQUIVO))
        vol = (esq.volume + dir_.volume) / 1000
        print('[%s] %s' % (nome, ARQUIVO))
        print('  furos da barra: %d (de %d a %d mm, a cada %d mm)'
              % (len(DIST), min(DIST), max(DIST), DIST[1] - DIST[0]))
        print('  braço com %.1f mm do fundo do pé até a ponta (ponta a %.1f mm do furo da placa)'
              % (FIM - PE_TRAS, FIM))
        print('  frente da barra no último furo a %.1f mm do furo da placa' % (max(DIST) + BARRA_FRENTE))
        print('  volume dos dois: %.2f cm3  ->  %.1f g em PLA maciço' % (vol, vol * 1.24))
        print('  ocupação na mesa (mm):', np.round(placa.bounds[1] - placa.bounds[0], 1))
