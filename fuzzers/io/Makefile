# TODO: factor out common parts

N=60
VIVADO?=/opt/Xilinx/Vivado/2020.2/bin/vivado

ITERS=$(shell seq 1 1 $(N))
FEATURES=$(patsubst %,work/io_%.features,$(ITERS))
TCL=$(patsubst %,work/io_%.tcl,$(ITERS))

all: work/correlate.log

work/io_%.tcl: generate_io.py
	mkdir -p work
	python3 $^ $*

work/io_%.dump: work/io_%.tcl
	rm -f "work/io_$*.failed"
	rm -f "work/io_$*.bit"
	(cd work && (INDEX=$* $(VIVADO) -nojou -nolog -mode batch -source io_$*.tcl || (rm -f "io_$*.bit" && touch io_$*.dump && touch io_$*.failed))) # waive failures

%.features: %.dump
	pypy3 -m meowtra_util.features.io_features $^ $@ $(patsubst %.dump,%_config.txt,$^)

work/correlate.log: $(FEATURES)
	../../meowtra/build/meowtra correlate -tiles HPIO_L -sites work/ > $@

.PRECIOUS: $(FEATURES) $(patsubst %,work/io_%.dump,$(ITERS))

