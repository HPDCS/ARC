CC=gcc
FLAGS=-g -Wall -pthread -lm
FLAGS:=$(FLAGS) -DCACHE_LINE_SIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE) -DN_CPU=$(shell grep -c ^processor /proc/cpuinfo)
#INCLUDE= -I include/ -I mm/ -I core/ -Istatistics/
#LIBS=-pthread -lm
EXECUTABLE=test

all:
	$(CC) $(FLAGS) register.c -o register.o -c
	$(CC) $(FLAGS) main.c -o main.o -c
	$(CC) $(FLAGS) register.o main.o -o test
clean:
	rm register.o
	rm main.o
	rm test
