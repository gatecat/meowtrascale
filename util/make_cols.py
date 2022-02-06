import os

def parse_frame(f):
    bus = (f >> 24) & 0x7
    half = (f >> 23) & 0x1
    row = (f >> 18) & 0x1F
    col = (f >> 8) & 0x3FF
    m = f & 0xFF
    return (bus, half, row, col, m)

def main():
    frames = []

    with open(os.environ['MEOW_WORK'] + '/null.frames', 'r') as f:
        for line in f:
            sl = line.strip().split(' ')
            if len(sl) == 0:
                continue
            frames.append((int(sl[0]), int(sl[1], 16)))

    frames = list(sorted(set(frames)))

    i = 0
    while i < len(frames):
        slr, frame = frames[i]
        b, h, r, c, m = parse_frame(frame)
        col_width = 1
        i += 1
        while i < len(frames) and \
            frames[i] == (slr, (frame & 0xFFFFFF00) | ((frame+col_width) & 0xFF)):
                i += 1
                col_width += 1
        print(f"bus={b:01d} half={h:01d} row={r:03d} col={c:04d} width={col_width}")

if __name__ == '__main__':
    main()
