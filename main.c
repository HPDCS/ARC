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

#define DIM 1

struct wf_register *reg;
unsigned long long val = 1;
unsigned int busy_write=0, busy_read=0,  end_write=0, end_read=0, size=0, count_write=0, duration=0;
unsigned int *count_read;
bool start = false, end = false;

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

void *run_write(void *args){
	unsigned int arr[size/4];
	unsigned int j=0, k, id;
	struct writer_slot *wr_slt;
	
	wr_slt = writer_init(reg);
	id = get_id(wr_slt);
	
	printf("[%u]WR: START\n", id);
	
	while(!start);
	
	while(!end || (end_write!=0 && count_write >= end_write)){
		//write		
		reg_write(wr_slt, arr);
		//busy loops
		k = busy_loop(busy_write);
		
		count_write++;
		//if((count_write%(end_write/10)) == 0) printf("WR: Iteration %10u\n",count_write);
	}
	
	//end = true;
	printf("[0]WR: END\n");
	
	pthread_exit(NULL);

}

void *run_read(void *args){
	unsigned int k,j;
	unsigned int *ll;
	struct reader_slot *rd_slt; 
	unsigned int id;
	rd_slt= reader_init(reg);
	id = get_id(rd_slt);
	printf("[%u]RD: START\n", id);
	
	//sleep(1);
	while(!start);

	while(!end || (end_read!=0 && count_read[id] >= end_read)){
		//read
		ll=reg_read(rd_slt);
		//busy loops
		k = busy_loop(busy_read);
		
		count_read[id]++;
		//if((count_read[id]%(end_read/5)) == 0) printf("[%u]RD: Iteration %10u\n",id,count_read[id]);

	}
	printf("[%u]RD: %u read operations performed\n", id,count_read[id]);
	
	pthread_exit(NULL);

}

int main(int argn, char *argv[]) {
	unsigned int i, ret, readers, writers, tot_count_read = 0;
    timer exec_time;

    if(argn < 7) {
        fprintf(stderr, "Usage: %s: n_writer n_reader end_write end_read busy_write busy_read size\n", argv[0]);
        exit(EXIT_FAILURE);
    }
      
    writers = atoi(argv[1]);
    readers = atoi(argv[2]);
    size = atoi(argv[3]);
    
    if(argn == 7){
		busy_write = atoi(argv[4]);
		busy_read = atoi(argv[5]);
		duration = atoi(argv[6]);
	}
	else if (argn == 8){
		busy_write = atoi(argv[4]);
		busy_read = atoi(argv[5]);		
		end_write = atoi(argv[6]);
		end_read = atoi(argv[7]);
	}
    
    pthread_t p_tid[writers + readers];    
	reg = reg_init(writers, readers, size);
	//reg = reg_init(writers, readers, DIM*sizeof(unsigned long long));
	
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
	printf("START TEST on REGISTER(%u,%u) of size %u:                                              +\n", writers, readers, size);
	printf("\t write operation: %u, %u:                                                     +\n", end_write, busy_write);
	printf("\t read  operation: %u, %u:                                                     +\n", end_read, busy_read);
	printf("+-----------------------------------------------------------------------------------+\n\n\n");

	timer_start(exec_time);
	start = true;
	if(duration > 0){
		sleep(duration);
		end = true;
	}
	
	for(i = 0; i < writers + readers; i++){
		pthread_join(p_tid[i], NULL);
	}

	printf("\n\n+-----------------------------------------------------------------------------------+\n");
	printf("TEST ENDED: %.5f seconds\n", (double)timer_value_seconds(exec_time));
	printf("TOTAL WRTE: %u\n", count_write);
	for(i = 0; i < readers; i++)  tot_count_read +=count_read[i];
	printf("TOTAL READ: %u\n", tot_count_read);
	printf("TOTAL OPER: %u\n", tot_count_read+count_write);
	printf("+-----------------------------------------------------------------------------------+\n\n\n");
	
	reg_free(reg);
}
