import os

class Pin:
    def __init__(self, line):
        sl = line.strip().split(",")
        self.name = sl[0]
        self.bank = int(sl[1]) if sl[1] != "" else None
        self.pin_func = sl[2]
        self.site = sl[3]
        self.site_type = sl[4]
        self.flags = set(sl[5:])

    def __str__(self):
        return self.name

    @property
    def general(self):
        return "GENERAL_PURPOSE" in self.flags

    @property
    def global_clk(self):
        return "GLOBAL_CLK" in self.flags

class DeviceIO:
    def __init__(self):
        self.pins = {}
        with open(os.environ['MEOW_WORK'] + "/raw_io.txt", "r") as f:
            for line in f:
                sl = line.strip()
                if "," not in sl:
                    continue
                pin = Pin(line)
                self.pins[pin.name] = pin
        self.avail_io = [p for _, p in sorted(self.pins.items(), key=lambda x: x[0]) if p.general and not p.global_clk]
        self.avail_clk = [p for _, p in sorted(self.pins.items(), key=lambda x: x[0]) if p.general and p.global_clk]
    def next_io(self):
        return self.avail_io.pop()
    def next_clk(self):
        return self.avail_clk.pop()
