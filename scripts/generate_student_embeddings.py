import argparse
import os
import sys

try:
    from PIL import Image
except Exception:
    Image = None

def load_image_embedding(path, grid, iw, ew):
    if Image is None:
        raise RuntimeError("Pillow not available; install with: pip install pillow")
    img = Image.open(path).convert('L').resize((grid, grid))
    pix = list(img.getdata())
    vals = [p / 255.0 for p in pix]
    w = grid
    h = grid
    sob_x = [[-1, 0, 1], [-2, 0, 2], [-1, 0, 1]]
    sob_y = [[-1, -2, -1], [0, 0, 0], [1, 2, 1]]
    gx = [0.0] * (w * h)
    gy = [0.0] * (w * h)
    for y in range(h):
        for x in range(w):
            sx = 0.0
            sy = 0.0
            for j in range(-1, 2):
                for i in range(-1, 2):
                    xi = x + i
                    yj = y + j
                    if xi < 0: xi = 0
                    if xi >= w: xi = w - 1
                    if yj < 0: yj = 0
                    if yj >= h: yj = h - 1
                    v = vals[yj * w + xi]
                    sx += sob_x[j + 1][i + 1] * v
                    sy += sob_y[j + 1][i + 1] * v
            gx[y * w + x] = sx
            gy[y * w + x] = sy
    edge = [abs(gx[i]) + abs(gy[i]) for i in range(w * h)]
    vec = [iw * vals[i] + ew * edge[i] for i in range(w * h)]
    n2 = sum(v * v for v in vec)
    if n2 <= 1e-12:
        vec = [1.0] * (w * h)
        n2 = float(w * h)
    inv = n2 ** 0.5
    vec = [v / inv for v in vec]
    return vec

def write_embedding(vec, out_path, precision):
    fmt = "{:." + str(precision) + "f}"
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(" ".join(fmt.format(v) for v in vec))

def process_single(input_path, output_path, grid, iw, ew, precision):
    vec = load_image_embedding(input_path, grid, iw, ew)
    write_embedding(vec, output_path, precision)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', type=str)
    ap.add_argument('--input-dir', type=str)
    ap.add_argument('--output', type=str)
    ap.add_argument('--output-dir', type=str)
    ap.add_argument('--grid-size', type=int, default=16)
    ap.add_argument('--intensity-weight', type=float, default=0.4)
    ap.add_argument('--edge-weight', type=float, default=0.6)
    ap.add_argument('--precision', type=int, default=6)
    args = ap.parse_args()

    if not args.input and not args.input_dir:
        print('Error: provide --input IMG or --input-dir DIR')
        sys.exit(2)
    if args.input and args.input_dir:
        print('Error: provide only one of --input or --input-dir')
        sys.exit(2)
    if args.input:
        if not args.output:
            base = os.path.basename(args.input)
            name, _ = os.path.splitext(base)
            out = name + '_embed.txt'
        else:
            out = args.output
        process_single(args.input, out, args.grid_size, args.intensity_weight, args.edge_weight, args.precision)
        print('Wrote', out)
    else:
        if not os.path.isdir(args.input_dir):
            print('Error: input-dir not found')
            sys.exit(2)
        out_dir = args.output_dir or args.input_dir
        os.makedirs(out_dir, exist_ok=True)
        exts = {'.png', '.jpg', '.jpeg', '.bmp', '.gif'}
        files = [f for f in os.listdir(args.input_dir) if os.path.splitext(f.lower())[1] in exts]
        if not files:
            print('No images found in', args.input_dir)
            sys.exit(0)
        for f in files:
            ip = os.path.join(args.input_dir, f)
            name, _ = os.path.splitext(f)
            op = os.path.join(out_dir, name + '_embed.txt')
            try:
                process_single(ip, op, args.grid_size, args.intensity_weight, args.edge_weight, args.precision)
                print('Wrote', op)
            except Exception as e:
                print('Skip', f, 'due to', str(e))

if __name__ == '__main__':
    main()

