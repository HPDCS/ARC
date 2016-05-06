CC=gcc
FLAGS=-g -Wall -pthread -lm
#INCLUDE= -I include/ -I mm/ -I core/ -Istatistics/
EXECUTABLE=test

all: test

test: main.o register.o
	$(CC) $(FLAGS) register.o main.o -o $(EXECUTABLE)
	
main.o : main.c
	$(CC) $(FLAGS) main.c -o main.o -c

register.o : register.c register.h timer.h	
	$(CC) $(FLAGS) register.c -o register.o -c
	
clean:
	rm *.o test
