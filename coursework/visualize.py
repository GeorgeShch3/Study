import sys
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from mpl_toolkits.mplot3d import Axes3D 

ROOM_W, ROOM_D, ROOM_H = 6.0, 6.0, 3.0
EPS = 0.15  

PALETTE = ['#2196F3', '#FF9800', '#4CAF50', '#E91E63',
           '#9C27B0', '#00BCD4', '#FF5722', '#8BC34A']

def load(path):
    try:
        return pd.read_csv(path)
    except FileNotFoundError:
        print(f"Файл не найден: {path}")
        sys.exit(1)

def id_column(df):
    for col in ('scan_id', 'scanner_id', 'id'):
        if col in df.columns:
            return col
    return None

def draw_room_wireframe(ax, w=ROOM_W, d=ROOM_D, h=ROOM_H):
    edges = [
        [[0,0,0],[w,0,0]], [[w,0,0],[w,d,0]],
        [[w,d,0],[0,d,0]], [[0,d,0],[0,0,0]],
        [[0,0,h],[w,0,h]], [[w,0,h],[w,d,h]],
        [[w,d,h],[0,d,h]], [[0,d,h],[0,0,h]],
        [[0,0,0],[0,0,h]], [[w,0,0],[w,0,h]],
        [[w,d,0],[w,d,h]], [[0,d,0],[0,d,h]],
    ]
    for e in edges:
        ax.plot([e[0][0],e[1][0]], [e[0][1],e[1][1]], [e[0][2],e[1][2]],
                color='#333', linewidth=0.8, alpha=0.4)

def scatter3d(ax, df, max_pts=5000, color=None):
    step = max(1, len(df) // max_pts)
    sub  = df.iloc[::step]
    idc  = id_column(df)
    if color is None and idc:
        for i, sid in enumerate(sorted(sub[idc].unique())):
            mask = sub[idc] == sid
            ax.scatter(sub[mask]['x'], sub[mask]['y'], sub[mask]['z'],
                       s=0.8, alpha=0.5, color=PALETTE[int(sid) % len(PALETTE)])
    else:
        ax.scatter(sub['x'], sub['y'], sub['z'],
                   s=0.8, alpha=0.5, color=color or PALETTE[0])

def scatter2d(ax, df, xi, yi, max_pts=8000, color=None):
    step = max(1, len(df) // max_pts)
    sub  = df.iloc[::step]
    idc  = id_column(df)
    if color is None and idc:
        for i, sid in enumerate(sorted(sub[idc].unique())):
            mask = sub[idc] == sid
            ax.scatter(sub[mask][xi], sub[mask][yi],
                       s=0.5, alpha=0.4, color=PALETTE[int(sid) % len(PALETTE)])
    else:
        ax.scatter(sub[xi], sub[yi],
                   s=0.5, alpha=0.4, color=color or PALETTE[0])
    ax.set_xlabel(xi + ' (м)'); ax.set_ylabel(yi + ' (м)')
    ax.set_aspect('equal'); ax.grid(True, linewidth=0.4)

def wall_stats(df, label):
    walls = [
        ('X=0',  'x', 0.0),
        (f'X={ROOM_W}', 'x', ROOM_W),
        ('Y=0',  'y', 0.0),
        (f'Y={ROOM_D}', 'y', ROOM_D),
        ('Z=0 (пол)',   'z', 0.0),
        (f'Z={ROOM_H} (потолок)', 'z', ROOM_H),
    ]
    print(f'\n{"─"*60}')
    print(f'  {label}  ({len(df)} точек)')
    print(f'{"─"*60}')
    print(f'  {"Стена":<22} {"Точек":>6}  {"σ (мм)":>8}  {"среднее (м)":>12}')
    print(f'  {"─"*54}')
    results = []
    for name, coord, val in walls:
        pts = df[np.abs(df[coord] - val) < EPS][coord]
        if len(pts) < 10:
            continue
        sigma = pts.std() * 1000
        mean  = pts.mean()
        print(f'  {name:<22} {len(pts):>6}  {sigma:>8.1f}  {mean:>12.4f}')
        results.append((name, len(pts), sigma, mean, val))
    return results

def compare_stats(res_before, res_after):
    print(f'\n{"─"*60}')
    print(f'  Сравнение σ (меньше = лучше)')
    print(f'{"─"*60}')
    print(f'  {"Стена":<22} {"σ до":>8}  {"σ после":>8}  {"улучшение":>10}')
    print(f'  {"─"*54}')
    for (n1,_,s1,_,_), (n2,_,s2,_,_) in zip(res_before, res_after):
        imp = (s1 - s2) / s1 * 100 if s1 > 0 else 0
        print(f'  {n1:<22} {s1:>8.1f}  {s2:>8.1f}  {imp:>9.1f}%')

def set_equal_3d(ax, df):
    ax.set_xlabel('X (м)'); ax.set_ylabel('Y (м)'); ax.set_zlabel('Z (м)')
    for axis in (ax.xaxis, ax.yaxis, ax.zaxis):
        axis.pane.fill = False
        axis.pane.set_edgecolor('none')
    mx = max(df['x'].max()-df['x'].min(),
             df['y'].max()-df['y'].min(),
             df['z'].max()-df['z'].min()) / 2
    for get, set_ in [(lambda d: (d['x'].max()+d['x'].min())/2, ax.set_xlim),
                      (lambda d: (d['y'].max()+d['y'].min())/2, ax.set_ylim),
                      (lambda d: (d['z'].max()+d['z'].min())/2, ax.set_zlim)]:
        mid = get(df); set_(mid - mx, mid + mx)

def save(name):
    plt.savefig(name, dpi=150, bbox_inches='tight')
    print(f'  Сохранено: {name}')

def mode_robot():
    print('\n=== Режим: robot ===')
    before = load('robot_before.csv')
    after  = load('robot_after.csv')

    res_b = wall_stats(before, 'ДО коррекции ICP')
    res_a = wall_stats(after,  'ПОСЛЕ коррекции ICP')
    if res_b and res_a:
        compare_stats(res_b, res_a)

    fig = plt.figure(figsize=(16, 7))
    fig.suptitle('Карта робота-пылесоса: до и после ICP', fontsize=13, fontweight='bold')
    for col, (df, title) in enumerate([
        (before, f'БЕЗ ICP — только одометрия ({len(before)} точек)'),
        (after,  f'С ICP — коррекция ({len(after)} точек)'),
    ]):
        ax = fig.add_subplot(1, 2, col+1, projection='3d')
        scatter3d(ax, df, color=PALETTE[col])
        draw_room_wireframe(ax)
        ax.set_title(title, fontsize=10)
        ax.view_init(elev=30, azim=-50)
        set_equal_3d(ax, after)
    plt.tight_layout()
    save('robot_3d.png')

    fig, axes = plt.subplots(2, 3, figsize=(16, 10))
    fig.suptitle('Проекции карты: до (синий) / после ICP (оранжевый)', fontsize=13, fontweight='bold')
    pairs = [('x','y'), ('x','z'), ('y','z')]
    for col, (xi, yi) in enumerate(pairs):
        scatter2d(axes[0][col], before, xi, yi, color=PALETTE[0])
        axes[0][col].set_title(f'До ICP — {xi.upper()}{yi.upper()}')
        scatter2d(axes[1][col], after,  xi, yi, color=PALETTE[1])
        axes[1][col].set_title(f'После ICP — {xi.upper()}{yi.upper()}')
    plt.tight_layout()
    save('robot_projections.png')

    fig, axes = plt.subplots(2, 3, figsize=(16, 8))
    fig.suptitle('Разброс точек по плоскостям стен (σ)', fontsize=13, fontweight='bold')
    walls = [('x', 0.0, 'X=0'), ('x', ROOM_W, f'X={ROOM_W}'),
             ('y', 0.0, 'Y=0'), ('y', ROOM_D, f'Y={ROOM_D}'),
             ('z', 0.0, 'Z=0 пол'), ('z', ROOM_H, f'Z={ROOM_H} потолок')]
    for i, (coord, val, name) in enumerate(walls):
        ax = axes[i // 3][i % 3]
        for df, label, color in [(before,'до',PALETTE[0]),(after,'после',PALETTE[1])]:
            pts = df[np.abs(df[coord] - val) < EPS][coord]
            if len(pts) > 5:
                ax.hist(pts, bins=50, alpha=0.6, label=f'{label} σ={pts.std()*1000:.1f}мм',
                        color=color, edgecolor='white')
        ax.axvline(val, color='red', linewidth=1.5, linestyle='--')
        ax.set_title(name); ax.legend(fontsize=8); ax.set_xlabel(f'{coord} (м)')
    plt.tight_layout()
    save('robot_walls.png')

    plt.show()

def mode_corner():
    print('\n=== Режим: corner ===')
    before = load('corners_before.csv')
    after  = load('corners_after.csv')

    MARGIN = 0.3; LH = ROOM_H / 2.0
    scanner_pos = [(MARGIN, MARGIN, LH), (ROOM_W-MARGIN, MARGIN, LH),
                   (MARGIN, ROOM_D-MARGIN, LH), (ROOM_W-MARGIN, ROOM_D-MARGIN, LH)]

    res_b = wall_stats(before, 'ДО сшивки (с калибровочной ошибкой)')
    res_a = wall_stats(after,  'ПОСЛЕ сшивки по плоскостям')
    if res_b and res_a:
        compare_stats(res_b, res_a)

    # ---- 3D ----
    fig = plt.figure(figsize=(16, 7))
    fig.suptitle('Статическое сканирование: до и после сшивки', fontsize=13, fontweight='bold')
    for col, (df, title) in enumerate([
        (before, f'ДО сшивки ({len(before)} точек)'),
        (after,  f'ПОСЛЕ сшивки по плоскостям ({len(after)} точек)'),
    ]):
        ax = fig.add_subplot(1, 2, col+1, projection='3d')
        scatter3d(ax, df)
        draw_room_wireframe(ax)
        for i, pos in enumerate(scanner_pos):
            ax.scatter(*pos, color=PALETTE[i], s=60, zorder=5, marker='^')
        ax.set_title(title, fontsize=10)
        ax.view_init(elev=25, azim=-50)
        ax.set_xlim(0, ROOM_W); ax.set_ylim(0, ROOM_D); ax.set_zlim(0, ROOM_H)
        ax.set_xlabel('X (м)'); ax.set_ylabel('Y (м)'); ax.set_zlabel('Z (м)')
    plt.tight_layout()
    save('corner_3d.png')

    fig, axes = plt.subplots(2, 3, figsize=(16, 10))
    fig.suptitle('Проекции и разброс по стенам', fontsize=13, fontweight='bold')
    for col, (xi, yi) in enumerate([('x','y'), ('x','z'), ('y','z')]):
        scatter2d(axes[0][col], before, xi, yi); axes[0][col].set_title(f'До — {xi.upper()}{yi.upper()}')
        scatter2d(axes[1][col], after,  xi, yi); axes[1][col].set_title(f'После — {xi.upper()}{yi.upper()}')
    plt.tight_layout()
    save('corner_projections.png')

    fig, axes = plt.subplots(2, 3, figsize=(16, 8))
    fig.suptitle('Разброс точек по плоскостям (σ)', fontsize=13, fontweight='bold')
    walls = [('x', 0.0, 'X=0'), ('x', ROOM_W, f'X={ROOM_W}'),
             ('y', 0.0, 'Y=0'), ('y', ROOM_D, f'Y={ROOM_D}'),
             ('z', 0.0, 'Z=0 пол'), ('z', ROOM_H, f'Z={ROOM_H} потолок')]
    for i, (coord, val, name) in enumerate(walls):
        ax = axes[i // 3][i % 3]
        for df, label, color in [(before,'до',PALETTE[0]),(after,'после',PALETTE[1])]:
            pts = df[np.abs(df[coord] - val) < EPS][coord]
            if len(pts) > 5:
                ax.hist(pts, bins=50, alpha=0.6,
                        label=f'{label} σ={pts.std()*1000:.1f}мм',
                        color=color, edgecolor='white')
        ax.axvline(val, color='red', linewidth=1.5, linestyle='--')
        ax.set_title(name); ax.legend(fontsize=8); ax.set_xlabel(f'{coord} (м)')
    plt.tight_layout()
    save('corner_walls.png')

    plt.show()

def mode_adaptive():
    print('\n=== Режим: adaptive ===')
    files = [
        ('uniform.csv',    'Равномерный (bias=0)',     PALETTE[0]),
        ('edge_dense.csv', 'Плотнее на границах (+0.8)', PALETTE[1]),
        ('flat_dense.csv', 'Плотнее в центре (-0.8)',  PALETTE[2]),
    ]
    datasets = []
    for fname, label, color in files:
        df = load(fname)
        datasets.append((df, label, color))
        print(f'  {label}: {len(df)} точек')

    fig = plt.figure(figsize=(18, 6))
    fig.suptitle('Адаптивная плотность точек', fontsize=13, fontweight='bold')
    for i, (df, title, color) in enumerate(datasets):
        ax = fig.add_subplot(1, 3, i+1, projection='3d')
        scatter3d(ax, df, color=color)
        ax.set_title(title, fontsize=10)
        ax.view_init(elev=25, azim=-40)
        set_equal_3d(ax, datasets[0][0])
    plt.tight_layout()
    save('adaptive_3d.png')

    fig, axes = plt.subplots(3, 3, figsize=(16, 13))
    fig.suptitle('Проекции: адаптивная плотность', fontsize=13, fontweight='bold')
    for row, (df, title, color) in enumerate(datasets):
        for col, (xi, yi) in enumerate([('x','y'), ('x','z'), ('y','z')]):
            scatter2d(axes[row][col], df, xi, yi, color=color)
            axes[row][col].set_title(f'{title} — {xi.upper()}{yi.upper()}', fontsize=9)
    plt.tight_layout()
    save('adaptive_projections.png')

    fig, ax = plt.subplots(figsize=(10, 5))
    fig.suptitle('Распределение дальностей', fontsize=13, fontweight='bold')
    for df, label, color in datasets:
        ax.hist(df['distance'], bins=80, alpha=0.6, label=label,
                color=color, edgecolor='white')
    ax.set_xlabel('Дальность (м)'); ax.set_ylabel('Количество точек')
    ax.legend(); ax.grid(True, linewidth=0.4)
    plt.tight_layout()
    save('adaptive_distances.png')

    plt.show()

def mode_file(path):
    print(f'\n=== Режим: file ({path}) ===')
    df = load(path)
    print(f'  Точек: {len(df)}')
    print(f'  Колонки: {list(df.columns)}')
    print(f'  X: [{df["x"].min():.3f}, {df["x"].max():.3f}]')
    print(f'  Y: [{df["y"].min():.3f}, {df["y"].max():.3f}]')
    print(f'  Z: [{df["z"].min():.3f}, {df["z"].max():.3f}]')

    stem = path.replace('.csv', '')

    fig = plt.figure(figsize=(10, 8))
    ax  = fig.add_subplot(111, projection='3d')
    scatter3d(ax, df)
    ax.set_title(path, fontsize=11)
    ax.view_init(elev=25, azim=-50)
    set_equal_3d(ax, df)
    plt.tight_layout()
    save(f'{stem}_3d.png')

    fig, axes = plt.subplots(1, 3, figsize=(16, 5))
    fig.suptitle(f'Проекции: {path}', fontsize=13, fontweight='bold')
    for ax, (xi, yi) in zip(axes, [('x','y'), ('x','z'), ('y','z')]):
        scatter2d(ax, df, xi, yi)
        ax.set_title(f'{xi.upper()}{yi.upper()}')
    plt.tight_layout()
    save(f'{stem}_projections.png')

    if 'distance' in df.columns:
        fig, ax = plt.subplots(figsize=(10, 4))
        ax.hist(df['distance'], bins=80, color=PALETTE[0], edgecolor='white', alpha=0.8)
        ax.set_xlabel('Дальность (м)'); ax.set_ylabel('Точек')
        ax.set_title('Распределение дальностей'); ax.grid(True, linewidth=0.4)
        plt.tight_layout()
        save(f'{stem}_distances.png')

    plt.show()

def main():
    parser = argparse.ArgumentParser(
        description='Визуализация облаков точек лидара',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры:
  python visualize.py robot          # robot_before/after.csv
  python visualize.py corner         # corners_before/after.csv
  python visualize.py adaptive       # uniform/edge_dense/flat_dense.csv
  python visualize.py obj points.csv # любой CSV файл
  python visualize.py file scan.csv  # то же самое
        """
    )
    parser.add_argument('mode', choices=['robot','corner','adaptive','obj','file'],
                        help='Тип сканирования')
    parser.add_argument('csv', nargs='?', default='points.csv',
                        help='CSV файл (для режимов obj/file)')
    args = parser.parse_args()

    plt.style.use('seaborn-v0_8-whitegrid')
    plt.rcParams.update({'figure.dpi': 100, 'font.size': 10})

    if   args.mode == 'robot':    mode_robot()
    elif args.mode == 'corner':   mode_corner()
    elif args.mode == 'adaptive': mode_adaptive()
    elif args.mode in ('obj', 'file'): mode_file(args.csv)

if __name__ == '__main__':
    main()