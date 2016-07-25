#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>

#include "timer.h"

#include "register.h"

struct register_slot *reg;
unsigned long long val = 1;
unsigned int busy_write=0, busy_read=0, end_write=0, end_read=0, size=0, count_write=0, duration=0,  load_reader=0, load_writer=0, rd_id=0;
unsigned int *count_read;
bool end = false, start = false;

void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;

    for (i=size-1;i>=0;i--)
    {
        for (j=7;j>=0;j--)
        {
            byte = b[i] & (1<<j);
            byte >>= j;
            printf("%u", byte);
        }
    }
    puts(" ");
}

int busy_loop(unsigned int count){
	unsigned int i, k;
	for(i=0; i<count*1000; i++){
		k = i * i;
	}
	return k;
}

void * run_write(void *args){
	unsigned int arr[size/4];
	unsigned int j=0, k;
	struct writer_slot *wr_slt;
	
	//printf("[0]WR: START\n");
	
	while(!start);
	
	while(!end || (end_write!=0 && count_write >= end_write)){
		//carico
		if(load_writer>0){
			for(j=0; j<size/4; j++) 
				arr[j]++;
		}
		//write		
		reg_write(reg, arr, size);
		//busy loops
		k = busy_loop(busy_write);
		
		count_write++;
		//if((count_write%(end_write/10)) == 0) printf("WR: Iteration %10u\n",count_write);
	}
	//printf("[0]WR: END\n");
	
	pthread_exit(NULL);	
}

void * run_read(void *args){
	unsigned int k,j, id, size_r=0, *ll;
	unsigned int arr[size/4];
	id = __sync_fetch_and_add(&rd_id, 1);
	//printf("[%u]RD: START\n", id);
	
	//sleep(1);

	//while(!start);

	while(!end || (end_read!=0 && count_read[id] >= end_read)){
		//read
		ll=reg_read(reg);
		//carico
		if(load_reader>0){
			for(j=0; j<size/4; j++) 
				arr[j]=ll[j];
		}
		//busy loops
		k = busy_loop(busy_read);
		
		count_read[id]++;
		//if((count_read[id]%(end_read/5)) == 0) printf("[%u]RD: Iteration %10u\n",id,count_read[id]);
	}
	//printf("[%u]RD: %u read operations performed\n", id,count_read[id]);
	
	pthread_exit(NULL);
}

int main(int argn, char *argv[]) {
	unsigned int i, ret, readers, writers, tot_count_read=0;
    timer exec_time;

    if(argn < 8) {
        fprintf(stderr, "Usage: %s: n_writer n_reader end_write end_read busy_write busy_read size\n", argv[0]);
        exit(EXIT_FAILURE);
    }
      
    writers = atoi(argv[1]);
    readers = atoi(argv[2]);
    size = atoi(argv[3]);
    
    if(argn == 9){
		busy_write = atoi(argv[4]);
		busy_read = atoi(argv[5]);
		duration = atoi(argv[6]);
		load_writer = atoi(argv[7]);
		load_reader = atoi(argv[8]);
	}
	else if (argn == 10){
		busy_write = atoi(argv[4]);
		busy_read = atoi(argv[5]);		
		end_write = atoi(argv[6]);
		end_read = atoi(argv[7]);
		load_writer = atoi(argv[8]);
		load_reader = atoi(argv[9]);
	}

    pthread_t p_tid[writers + readers];    
	reg = reg_init(writers, readers, size);
	//reg = reg_init(writers, readers, sizeof(unsigned long long));
    
    if((count_read = calloc(readers, sizeof(unsigned int))) == NULL){
		printf("malloc failed\n");
		abort();
	}
    
    for(i = 0; i < writers; i++) {
        if( (ret = pthread_create(&p_tid[i], NULL, run_write, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
    }
    for(i = 0; i < readers; i++) {
        if( (ret = pthread_create(&p_tid[writers+i], NULL, run_read, NULL)) != 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            abort();
        }
    }
    
	sleep(1);
	printf("\n\n+-----------------------------------------------------------------------------------+\n");
	printf("START TEST on REGISTER(%u,%u) of size %u for %u seconds:\n\n", writers, readers, size, duration);

	timer_start(exec_time);
	start = true;
	if(duration > 0){
		sleep(duration);
		end = true;
	}
	
	for(i = 0; i < writers + readers; i++){
		pthread_join(p_tid[i], NULL);
	}

	printf("TOTAL WRTE: %u\n", count_write);
	for(i = 0; i < readers; i++)  tot_count_read +=count_read[i];
	printf("TOTAL READ: %u\n", tot_count_read);
	printf("TOTAL OPER: %u\n", tot_count_read+count_write);
	printf("+-----------------------------------------------------------------------------------+\n\n\n");
	
    reg_free(reg);
}
