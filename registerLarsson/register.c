#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>

#define NRDR_MASK 0X00000000ffffffffLL	// mask of the reader counter
#define PTRFIELDLEN 6LL
#define PTRFIELD 0X000000000000003fLL


struct register_slot {
	unsigned int r_started; 	// number of read operation started on the slot
	unsigned int r_endend; 		// number of read operation ended on the slot
	unsigned int size;			// size of memory of the data associated to the slot (used for variable size register)
	void *value_loc; 			// pointer to the register value corresponding to this slot
};

struct wf_register {
	unsigned long long current; 		// composed by [32 bit of index ||32 bit of counter] 
	struct register_slot *rw_space;	// curcular buffer of register slot to use for the current value
	unsigned int size_slot;			// size of the register. Only for fixed size
	unsigned int writers;			// max number of concurrent writers, fixed in the init
	unsigned int readers;			// max number of concurrent readers, fixed in the init
	unsigned int readers_registered;			// max number of concurrent readers, fixed in the init
	unsigned int *tracer;
} ;

void *_reg_write(struct wf_register *reg, void *value, unsigned int size);

/**
* Init the register allocating the needed amount of memory and calculate the base for the current value.
*
* @author Mauro Ianni
* @param the number of writers and readers, and the size of the register
* @return a pointer to the instance of the register
* @date 24/02/2016
*/
struct wf_register *_reg_init(unsigned int n_wrts, unsigned int n_rdrs, unsigned int size){	
	//TODO: prima scrittura a 0 o ad un valore dato	
	unsigned int i;
	struct wf_register *reg;
 	if((reg = malloc(sizeof(struct wf_register)))==NULL){
		printf("malloc failed\n");
        abort();
	}			
	reg->readers = n_rdrs;	
	reg->writers = n_wrts;	
	reg->size_slot = size;		
	reg->current = 0;
	reg->readers_registered = 0;
	
	if((reg -> rw_space = calloc(reg->readers+2, sizeof(struct register_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	
	if((reg -> tracer = calloc(reg->readers, sizeof(unsigned int)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	
	/* if the register is static, the memory is preallocated */
	if(size != 0){
		for(i = 0; i < n_rdrs+2; i++){
			reg->rw_space[i].size = size;
			if((reg->rw_space[i].value_loc = malloc(size))==NULL){
				printf("malloc failed\n");
				abort();
			}
		}
	}
	/* if the register is dynamic */
	
	/*_reg_write(reg, &reg->current, 8);
	else{
		for(i = 0; i < n_rdrs+2; i++){
			reg->rw_space[i].value_loc = NULL;
		}
	}*/
	
	return reg;
}	

unsigned int reader_init(struct wf_register *reg){
	unsigned int id;
	id = __sync_fetch_and_add(&reg->readers_registered, 1);
	if(id >= 58) 
		return ~0;
	else 
		return id;
}

void *_reg_write(struct wf_register *reg, void *val, unsigned int size){
	unsigned int tmp_size, tr, size_slot;
	unsigned long long current_index, i, j, wsync, oldwptr;
	
	current_index = reg->current & PTRFIELD;
	i = 0;//(current_index + 1) % (reg->readers + 2);
	
	while(1){
		if(current_index != i){
			tr = 0;
			for(j = 0; j < reg->readers ; j++){
				if(reg->tracer[j] == i) tr++;
			}
		}
		if(tr == 0) break;
		i++;
		i = i % (reg->readers + 2); 
	}
	/* allocate memory for the new buffer. It is possible also to reuse memory but also for fixed size and this can produce memory unefficiency */
	size_slot = reg->size_slot; 
	if(size_slot==0 && reg->rw_space[i].size != size){
		free(reg->rw_space[i].value_loc);
		if((reg->rw_space[i].value_loc = malloc(size))==NULL){
			printf("malloc failed\n");
			abort();
		}
	}
	
	tmp_size = reg->rw_space[i].size;
	tmp_size = (size_slot > 0 ? size_slot:size);
	//printf("%u\n",tmp_size);
	memcpy(reg->rw_space[i].value_loc, val, tmp_size);
	
	wsync = __sync_lock_test_and_set(&reg->current, i);
	
	oldwptr = wsync & PTRFIELD;
	
	for(j = 0; j < reg->readers ; j++)
		if(wsync & (1 << (j + PTRFIELDLEN)))	
			reg->tracer[j] = oldwptr;

	return reg->rw_space[i].value_loc; 
}


/**
* Read th value of the register
*
* @author Mauro Ianni
* @param the pointer to the register, the address of the value to write and it's size
* @return the pointer to a memory location with a copy of the current value
* @date 24/02/2016
*/
void *reg_read(struct wf_register *reg, unsigned int id, unsigned int *size){
	unsigned long long readerbit, rsync, rptr, id_abs;
	void *mem_loc, *mem_ret;
	
	id_abs = (id + PTRFIELDLEN);
	readerbit = 1LL << id_abs;
	rsync = __sync_fetch_and_or(&reg->current, readerbit);
	rptr = rsync & PTRFIELD;
	/*
	if((mem_ret = malloc(reg->size_slot))==NULL){
		printf("malloc failed\n");
        abort();
	}
	/* read from the memory location */
	mem_loc = reg->rw_space[rptr].value_loc;
	/*
	memcpy(mem_ret, mem_loc, reg->rw_space[rptr].size);
	*/
	return mem_loc;//mem_ret;
}

/**
* Free the memory used by the register
*
* @author Mauro Ianni
* @param the pointer to the register
* @return void
* @date 24/02/2016
*/
void reg_free(struct wf_register *reg){
	free(reg->rw_space);
	free(reg);
}


