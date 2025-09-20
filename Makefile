GCC = gcc
CFLAGS = -Wall -Werror
CFLAGS += -O3
#CFLAGS += -DDEBUG

.PHONY: all
all:
	$(GCC) $(CFLAGS) main.c huffman.c file_buffer.c mem_buffer.c -o huffman
	$(GCC) $(CFLAGS) main_parallel.c huffman.c file_buffer.c mem_buffer.c -o huffman_parallel

.PHONY: clean  
clean:
	rm -rf huffman huffman_parallel out*