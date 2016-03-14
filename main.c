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
bool end = false;

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

void *run_write(void *args){
	unsigned long long arr[DIM];
	unsigned int i, size, id;
	struct writer_slot *wr_slt;
	wr_slt = writer_init(reg);
	size=DIM*sizeof(unsigned long long);
	id = get_id(wr_slt);
	
	printf("[%u]WR: START\n", id);
	
	for( i = 0; i < 40000000 ; i++){
		
		arr[0] = ++val;
		
		//reg_write(wr_slt, &val, size); 		
		reg_write(wr_slt, arr, size); 		

		if((i%10000000) == 0) printf("WR: Iteration %10u\n",i);
	}
	end = true;
	printf("WR: END\n");
	
	pthread_exit(NULL);

}

void *run_read(void *args){
	unsigned long long *ll, old=0, i=0, val1, val2;
	struct reader_slot *rd_slt; 
	unsigned int id;
	rd_slt= reader_init(reg);
	id = get_id(rd_slt);
	printf("[%u]RD: START\n", id);
	
	sleep(1);

	while(!end){
		i++;
		val1=val;
		ll=reg_read(rd_slt);
		val2=val;
		if(ll[0] < val1-1 || ll[0] > val2){
			printf("[%u]RD: val1=%llu letto=%llu val1=%llu\n", id, val1, ll[0], val2);
		}
		
		old= ll[0];
		if((i%10000000) == 0) printf("[%u]RD: Iteration %10llu\n",id,i);
		
	}
	printf("[%u]RD: %llu read operations performed\n", id,i);
	
	pthread_exit(NULL);

}

int main(int argn, char *argv[]) {
	unsigned int i, ret, readers, writers;
    timer exec_time;

    if(argn < 3) {
        fprintf(stderr, "Usage: %s: n_writer n_reader\n", argv[0]);
        exit(EXIT_FAILURE);
    }
      
    writers = atoi(argv[1]);
    readers = atoi(argv[2]);
    pthread_t p_tid[writers + readers];    
	reg = reg_init(writers, readers);
	//reg = reg_init(writers, readers, DIM*sizeof(unsigned long long));
    
    printf("Start test on Register(%u,%u):\n", writers, readers);

    timer_start(exec_time);
    
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
    
    for(i = 0; i < writers + readers; i++){
        pthread_join(p_tid[i], NULL);
    }

    printf("test ended: %.5f seconds\n", (double)timer_value_seconds(exec_time));
    reg_free(reg);
}
