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


struct wf_register {
	void **copybuff;	// curcular buffer of register slot to use for the current value
	void *BUFF1;
	void *BUFF2;
	void **buf1_loc;	// curcular buffer of register slot to use for the current value
	void **buf2_loc;	// curcular buffer of register slot to use for the current value
	
	unsigned int size_slot;			// size of the register. Only for fixed size
	unsigned int writers;			// max number of concurrent writers, fixed in the init
	unsigned int readers;			// max number of concurrent readers, fixed in the init
	unsigned int readers_registered;			// max number of concurrent readers, fixed in the init
	
	bool wflag;
	bool switc;
	bool *reading;
	bool *writing;
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
	reg->readers_registered = 0;
	
	reg->wflag = reg->switc = false;
	
	if((reg -> reading = malloc(reg->readers*sizeof(bool)))==NULL || (reg -> writing = malloc(reg->readers*sizeof(bool)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	
		
	if((reg -> BUFF1 = malloc(size)) == NULL || (reg -> BUFF2 = malloc(size)) == NULL){
		printf("malloc failed\n");
        abort();
	}
	
	if((reg->copybuff = malloc(sizeof(void*) * n_rdrs))==NULL){
		printf("malloc failed\n");
        abort();
	}
	if((reg->buf1_loc = malloc(sizeof(void*) * n_rdrs))==NULL){
		printf("malloc failed\n");
        abort();
	}
	if((reg->buf2_loc = malloc(sizeof(void*) * n_rdrs))==NULL){
		printf("malloc failed\n");
        abort();
	}
	for(i=0;i<n_rdrs;i++){
		 	if((reg->copybuff[i] = malloc(size))==NULL || (reg->buf1_loc[i] = malloc(size))==NULL || (reg->buf2_loc[i] = malloc(size))==NULL){
				printf("malloc failed\n");
				abort();
			}		
	}
	
	return reg;
}	

unsigned int reader_init(struct wf_register *reg){
	unsigned int id;
	id = __sync_fetch_and_add(&reg->readers_registered, 1);
	return id;
}

void *_reg_write(struct wf_register *reg, void *val, unsigned int size){
	unsigned int i;
		
	reg->wflag = true;
	memcpy(reg->BUFF1, val, reg->size_slot);
	reg->switc = (!reg->switc);
	reg->wflag = false;
	
	for(i = 0 ; i < reg->readers ; i++){
		if(reg->reading[i] != reg->writing[i]){
			memcpy(reg->copybuff[i], val, reg->size_slot);
			reg->writing[i] = reg->reading[i];
		}
	}
	memcpy(reg->BUFF2, val, reg->size_slot);
}


void *reg_read(struct wf_register *reg, unsigned int id, unsigned int *size){
	bool flag1, sw1, flag2, sw2;
	//void *buf1, *buf2;
	
	//buf1=malloc(reg->size_slot);
	//buf2=malloc(reg->size_slot);
	
	reg->reading[id]= (!reg->writing[id]);
	
	flag1 = reg->wflag;
	sw1 = reg->switc;
	memcpy(reg->buf1_loc[id], reg->BUFF1, reg->size_slot);
	flag2 = reg->wflag;
	sw2 = reg->switc;
	memcpy(reg->buf2_loc[id], reg->BUFF2, reg->size_slot);
	
	if(reg->reading[id] == reg->writing[id]){
		//free(buf1);
		//free(buf2);
		return reg->copybuff[id];
	}
	else if((sw1 != sw2) || flag1 || flag2){
		//free(buf1);
		return reg->buf2_loc[id];
	}
	else{
		//free(buf2);
		return reg->buf1_loc[id];
	}
}

void reg_free(struct wf_register *reg){
	free(reg);
}


