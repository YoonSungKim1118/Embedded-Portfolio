CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -Wconversion -O2 -Iinclude
BUILD   := build
SRC     := src/ringbuf.c src/crc16.c src/rs485_frame.c
TEST    := tests/test_rs485_frame.c

.PHONY: all test clean

all: $(BUILD)/test_rs485_frame

$(BUILD)/test_rs485_frame: $(SRC) $(TEST) | $(BUILD)
	$(CC) $(CFLAGS) $(SRC) $(TEST) -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(BUILD)/test_rs485_frame
	./$(BUILD)/test_rs485_frame

clean:
	rm -rf $(BUILD)
