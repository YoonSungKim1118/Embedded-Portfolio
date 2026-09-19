CC     ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -Iruntime -Iexample
PY     ?= python3
BUILD  := build

.PHONY: all test budget clean

all: test

example/model.npz:
	cd example && $(PY) train_synth.py

example/weights.h: example/model.npz export_c.py
	cd example && $(PY) ../export_c.py model.npz -o weights.h

example/refcase.h: example/model.npz
	cd example && $(PY) gen_refcase.py

$(BUILD)/verify: runtime/mlp.c example/main.c example/weights.h example/refcase.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) runtime/mlp.c example/main.c -o $@ -lm

test: $(BUILD)/verify
	./$(BUILD)/verify

budget:
	$(PY) budget.py --mhz 120 --deadline-us 1000 --in 8 --out 2 --max-nodes 64 --top 5

clean:
	rm -rf $(BUILD) example/weights.h example/refcase.h \
	       example/model.npz example/refcase.npz
