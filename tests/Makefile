# Host-side unit tests for the hardware-independent LoRa code.
# Run with: make -C tests
CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -std=c11 -g -fsanitize=address,undefined

SRC = test_lora.c ../lora/lora_proto.c ../lora/lora_config.c

all: test
	./test_lora

test: $(SRC) ../lora/lora_proto.h ../lora/lora_config.h
	$(CC) $(CFLAGS) -I../lora -o test_lora $(SRC)

clean:
	rm -f test_lora

.PHONY: all test clean
