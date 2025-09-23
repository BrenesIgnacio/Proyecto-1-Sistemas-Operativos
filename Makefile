GCC = gcc
CFLAGS = -Wall -Werror
CFLAGS += -O3
#CFLAGS += -DDEBUG

.PHONY: all
all:
	$(GCC) $(CFLAGS) main.c huffman.c file_buffer.c mem_buffer.c -o huffman
	$(GCC) $(CFLAGS) main_fork.c huffman.c file_buffer.c mem_buffer.c -o huffman_fork
	$(GCC) $(CFLAGS) main_pthread.c huffman.c file_buffer.c mem_buffer.c -o huffman_pthread -lpthread
	

.PHONY: clean  
clean:
	rm -rf huffman huffman_fork huffman_pthread huffman_parallel out*
