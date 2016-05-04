#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>

#define NRDR_MASK 0X00000000ffffffff	// mask of the reader counter

//Qui chi ci rimette sono i reader, non so se va bene...

struct register_slot {
	unsigned int size;			// size of memory of the data associated to the slot (used for variable size register)
	void *value_loc; 			// pointer to the register value corresponding to this slot
	int lock;
	int wr_lk;
};

/*
* Init the register allocating the needed amount of memory and calculate the base for the current value.
*
* @author Mauro Ianni
* @param the number of writers and readers, and the size of the register
* @return a pointer to the instance of the register
* @date 24/02/2016
*/
struct register_slot *_reg_init(unsigned int n_wrts, unsigned int n_rdrs, unsigned int size){	
	//TODO: prima scrittura a 0 o ad un valore dato	
	struct register_slot *reg;
 	if((reg = malloc(sizeof(struct register_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}			
	
	reg->size = size;		
	
	if((reg->value_loc = calloc(1,size))==NULL){
		printf("malloc failed\n");
		abort();
	}
	
	reg->lock = 0;
	reg->wr_lk = 0;
	
	return reg;
}	

void *_reg_write(struct register_slot *reg, void *val, unsigned int size){
	
	reg->wr_lk = 1; //do priorità al writer
	
	while(__sync_val_compare_and_swap(&reg->lock, 0, -1) != 0);
	
	reg->size = size;
	memcpy(reg->value_loc, val, size);
	
	reg->lock = reg->wr_lk = 0;
	
	return reg->value_loc; //forse per correttezza poi sarà meglio toglierlo
}

void *reg_read(struct register_slot *reg){
	int lk;
	void *mem_ret;

	while(1){
		if((lk = reg->lock) < 0 || reg->wr_lk == 1)
			continue;
		if(__sync_val_compare_and_swap(&reg->lock, lk, lk + 1) == lk)
			break;		
	}

	mem_ret = reg->value_loc;
	
	__sync_add_and_fetch(&reg->lock, -1);
	
	return mem_ret;
}

/**
* Free the memory used by the register
*
* @author Mauro Ianni
* @param the pointer to the register
* @return void
* @date 24/02/2016
*/
void reg_free(struct register_slot *reg){
	free(reg->value_loc);
	free(reg);
}


