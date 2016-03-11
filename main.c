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

struct wf_register *reg;
unsigned long long val;
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
	//unsigned long long val;
	unsigned long long *ll;
	char arr[256];
	unsigned int i;
	struct writer_slot *wr_slt;
	wr_slt = writer_init(reg);
	
	printf("WR: START\n");
	
	val=0;

	for( i = 0; i < 40000000 ; i++){
		//ll=reg_write(wr_slt, &val, 8); 		
		ll=reg_write(wr_slt, arr, 256); 		

		val += 1;//(unsigned long long) (random() * 2);
		if((i%10000000) == 0) printf("WR: Iteration %10u\n",i);
	}
	end = true;
	printf("WR: END\n");
	
	pthread_exit(NULL);

}

void *run_read(void *args){
	unsigned long long *ll, old=0, i=0;
	struct reader_slot *rd_slt; 
	//unsigned int size;
	rd_slt= reader_init(reg);
	printf("RD: START\n");
	
	sleep(1);

	while(!end){
		i++;
		ll=reg_read(rd_slt);
		/*if(*ll!=0 && *ll < old){
			printf("val=%llu letto=%llu\n", old, *ll);
		}
		
		old= (*ll);
		if((i%10000000) == 0) printf("RD: Iteration %10llu\n",i);
		* */
	}
	printf("RD: %llu read operations performed\n", i);
	
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
	//reg = reg_init(writers, readers, sizeof(unsigned long long));
    
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
