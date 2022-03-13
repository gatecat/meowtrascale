# TODO: factor out common parts

N=300
VIVADO?=/opt/Xilinx/Vivado/2020.2/bin/vivado

ITERS=$(shell seq 1 1 $(N))
FEATURES=$(patsubst %,work/rclk_%.features,$(ITERS))
TCL=$(patsubst %,work/gen_design_%.tcl,$(ITERS))

all: work/correlate.log

work/generate.stamp: ../../meowtra/build/meowtra
	mkdir -p work
	../../meowtra/build/meowtra fuzztools -nodegraph ../../work/zu7ev/clk_graph.txt -out ./work -num-designs $(N)
	touch $@

$(TCL): work/generate.stamp

work/rclk_%.dump: work/gen_design_%.tcl
	rm -f "work/rclk_$*.failed"
	rm -f "work/rclk_$*.bit"
	(cd work && (INDEX=$* $(VIVADO) -nojou -nolog -mode batch -source ../run.tcl || (rm -f "rclk_$*.bit" && touch rclk_$*.dump && touch rclk_$*.failed))) # waive failures

%.features: %.dump
	pypy3 -m meowtra_util.features.route_features $^ $@

work/correlate.log: $(FEATURES)
	../../meowtra/build/meowtra correlate -tiles 'RCLK*,CMT*' -filter EXCL_CMT_DRP work/ > $@

.PRECIOUS: $(FEATURES) $(patsubst %,work/rclk_%.dump,$(ITERS))

