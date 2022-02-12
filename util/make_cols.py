import os
from .tilegrid import Tilegrid
from .tile_types import get_size

def parse_frame(f):
    bus = (f >> 24) & 0x7
    half = (f >> 23) & 0x1
    row = (f >> 18) & 0x1F
    col = (f >> 8) & 0x3FF
    m = f & 0xFF
    return (bus, half, row, col, m)

def main():
    grid = Tilegrid()
    frames = []

    with open(os.environ['MEOW_WORK'] + '/null.frames', 'r') as f:
        for line in f:
            sl = line.strip().split(' ')
            if len(sl) == 0:
                continue
            frames.append((int(sl[0]), int(sl[1], 16)))

    frames = list(sorted(set(frames)))

    # Group contiguous frames
    frame_groups = []
    frame_colsegs = {}
    i = 0
    while i < len(frames):
        slr, frame = frames[i]
        col_width = 1
        i += 1
        while i < len(frames) and \
            frames[i] == (slr, (frame & 0xFFFFFF00) | ((frame+col_width) & 0xFF)):
                i += 1
                col_width += 1
        frame_groups.append((slr, frame, col_width))
        b, h, r, c, m = parse_frame(frame)
        if b == 0:
            frame_colsegs[(r, c)] = (frame, col_width)

    # Dump frame ranges as they are needed for early bitstream parsing
    with open(os.environ['MEOW_DEVDB'] + '/frame_regions.txt', 'w') as f:
        for group in frame_groups:
            print(f"{group[0]} 0x{group[1]:08x} {group[2]}", file=f)

    # Sort tiles by clock row
    def include_tile(tt):
        return get_size(tt) is not None or tt.startswith("RCLK")

    bit_tiles = [[] for i in range(grid.clock_height)]
    for col in range(grid.width):
        col_segments = [[] for i in range(grid.clock_height)]
        for row in reversed(range(grid.height)):
            if (col, row) not in grid.tiles:
                continue
            t, tt, cr = grid.tiles[(col, row)]
            if cr is None:
                continue
            if not include_tile(tt):
                continue
            col_segments[cr[1]].append((t, tt))
        for clk_row, seg in enumerate(col_segments):
            if len(seg) > 0:
                bit_tiles[clk_row].append(seg)
    # Correlate between frames and tiles
    with open(os.environ['MEOW_DEVDB'] + '/tile_bits.txt', 'w') as f:
        for clk_row, col_segs in enumerate(bit_tiles):
            bit_col = 0
            if "xczu" in os.environ["MEOW_PART"]:
                # TODO: don't hardcode offset for tiles eaten by the PS part
                if clk_row <= 3:
                    bit_col = 0x56
            for seg in col_segs:
                t0, tt0 = seg[0]
                if tt0.startswith("RCLK"):
                    continue # ignore RCLK-only...
                w0, h0 = get_size(tt0)
                frame, frame_width = frame_colsegs[(clk_row, bit_col)]
                assert w0 == frame_width, (t0, w0, frame, frame_width)
                bit_col += 1
                start_bit = 0
                i = 0
                while i < len(seg):
                    ti, tti = seg[i]
                    seg_height = 1
                    i += 1
                    while i < len(seg) and seg[i][1] == tti:
                        i += 1
                        seg_height += 1
                    if tti.startswith("RCLK"):
                        wi, hi = (w0, 1) # RCLK always height1
                    else:
                        wi, hi = get_size(tti)
                    print(f"0 0x{frame:08x} +{frame_width} {ti} {hi}*{seg_height} {start_bit}", file=f) # TODO: SLR
                    start_bit += (hi * seg_height * 48)
                    if start_bit == 1488:
                        # skip over ECC
                        start_bit = 1536
if __name__ == '__main__':
    main()
