CC=gcc
FLAGS=-g -Wall -pthread -lm #a parte g, non so cosa siano
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
