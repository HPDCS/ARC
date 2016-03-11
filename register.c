#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>

#define NRRD_MASK 0X00000000ffffffff	// mask of the reader counter
#define NRRD_LENG 32

struct wf_register {
	unsigned long long current; 		// composed by [32 bit of index ||32 bit of counter] 
	struct register_slot *rw_space;		// curcular buffer of register slot to use for the current value
	unsigned int size_slot;				// size of the register. Only for fixed size
	char *current_reader;				// bitmap of the reader of the current copy
	unsigned int writers;				// max number of concurrent writers, fixed in the init
	unsigned int readers;				// max number of concurrent readers, fixed in the init
	unsigned int writers_registered;	// number of writers registered
	unsigned int readers_registered;	// number of readers registered
};

struct register_slot {
	unsigned int r_started; 			// number of read operation started on the slot
	unsigned int r_endend; 				// number of read operation ended on the slot
	unsigned int size;					// size of memory of the data associated to the slot (used for variable size register)
	void *value_loc; 					// pointer to the register value corresponding to this slot
};

struct reader_slot{
	unsigned int id;					// ID associated to the reader in the init phase
	struct wf_register *parent;				// address of the struct of the referred register
	unsigned int current;				// index of the slot to wich the the calue_loc is referred
	unsigned int size;					// size of memory of the data associated to the slot (used for variable size register)
	void *value_loc; 					// pointer to the register value corresponding to this slot
};

struct writer_slot{
	unsigned int id;					// ID associated to the writer in the init phase
	struct wf_register *parent;			// address of the struct of the referred register
	char *old_bitmap;					// personal copy of the bitmap
};

void *_reg_write(struct writer_slot *reg, void *value, unsigned int size);

/**
* Init the register allocating the needed amount of memory and calculate the base for the current value.
*
* @author Mauro Ianni
* @param the number of writers and readers, and the size of the register
* @return a pointer to the instance of the register
* @date 24/02/2016
*/
struct wf_register *_reg_init(unsigned int n_wrts, unsigned int n_rdrs, unsigned int size){	

	unsigned int i;
	struct wf_register *reg;
 	if((reg = malloc(sizeof(struct wf_register)))==NULL){
		printf("malloc failed\n");
        abort();
	}			
	reg->readers = n_rdrs;	
	reg->writers = n_wrts;
	reg->readers_registered = 0;	
	reg->writers_registered = 0;	
	reg->size_slot = size;		
	reg->current = 0;
	
	if((reg->rw_space = calloc(reg->readers+2, sizeof(struct register_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}	
	/* if the register is static, the memory is preallocated */
	if(size != 0){
		for(i = 0; i < n_rdrs + 2; i++){
			if((reg->rw_space[i].value_loc = malloc(size))==NULL){
				printf("malloc failed\n");
				abort();
			}
			reg->rw_space[i].size = size;
		}
	}	
	/* allocate space for the bitmap */		
	if((reg->current_reader = calloc(1, (reg->readers) / sizeof(char) + 1)) == NULL){
		printf("malloc failed\n");
		abort();
	}
	/* At the begin the register is setted to zero */
	if(size > 0)
		memset(reg->rw_space[i].value_loc, 0, size);
	
	return reg;
}	

/**
* Initialize the structure to make a reader working
*
* @author Mauro Ianni
* @param the pointer to the register
* @return A pointer to the structure used by the reader to work with the register
* @date 10/03/2016
*/
struct reader_slot *reader_init(struct wf_register *reg){
	struct reader_slot *rd_slt;
	unsigned int id;
	
	id = __sync_fetch_and_add(&(reg->readers_registered),1);
	if(id >= reg->readers){
		__sync_fetch_and_add(&(reg->readers_registered),-1);
		return NULL;
	}
		
	if((rd_slt = malloc(sizeof(struct reader_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	rd_slt->id = id;
	rd_slt->parent = reg;
	rd_slt->current = reg->current; //<- dovrebbe essere idnifferente
	
	rd_slt->size = reg->size_slot;
	
	if(reg->size_slot > 0){
		if((rd_slt->value_loc = malloc(reg->size_slot))==NULL){ // <- in caso di size 0 su mac potrebbe non funzionare...che peccato...
			printf("malloc failed\n");
			abort();
		} 
	}
		
	return rd_slt;
}

/**
* Initialize the structure to make a writer working
*
* @author Mauro Ianni
* @param the pointer to the register
* @return A pointer to the structure used by the writer to work with the register
* @date 10/03/2016
*/
struct writer_slot *writer_init(struct wf_register *reg){
	struct writer_slot *wr_slt;
	unsigned int id;
		
	id = __sync_fetch_and_add(&(reg->writers_registered),1);
	if(id >= reg->writers){
		__sync_fetch_and_add(&(reg->writers_registered),-1);
		return NULL;//return ~0;//id = 0;
	}
	
	if((wr_slt = malloc(sizeof(struct writer_slot)))==NULL){
		printf("malloc failed\n");
        abort();
	}
	wr_slt->id = id;
	wr_slt->parent = reg;
	
	if((wr_slt->old_bitmap = malloc((reg->readers) / sizeof(char) + 1)) == NULL){
		printf("malloc failed\n");
		abort();
	}
	
	return wr_slt;
}

/**
* Write size bytes from the address val in the register
*
* @author Mauro Ianni
* @param the pointer to the writer register slot, the address of the value to write and it's size
* @return void
* @date 10/03/2016
*/
void *_reg_write(struct writer_slot *wr_slt, void *val, unsigned int size){
	unsigned int tmp_size;
	unsigned long long current_tmp, current_index, i;
	struct wf_register * reg;
	
	reg = wr_slt->parent;
	
	current_index = reg->current >> NRRD_LENG;
	i = (current_index + 1) % (reg->readers + 2);
	/* search a free slot to use staring from the last one*/
	while(1){
		if((reg->rw_space[i].r_started == reg->rw_space[i].r_endend) && (current_index != i)){
			break;
		}
		i = (i + 1) % (reg->readers + 2); 
	}
	tmp_size = reg->size_slot;		
	/* allocate memory for the new buffer. It is possible also to reuse memory but also for fixed size and this can produce memory unefficiency */
	if(tmp_size==0 && reg->rw_space[i].size != size){
		free(reg->rw_space[i].value_loc);
		if((reg->rw_space[i].value_loc = malloc(size))==NULL){
			printf("malloc failed\n");
			abort();
		}
	}
	/* copy the value in the register and update the size*/	
	reg->rw_space[i].size = tmp_size = (tmp_size > 0 ? tmp_size:size);
	memcpy(reg->rw_space[i].value_loc, val, tmp_size);
	reg->rw_space[i].r_started = reg->rw_space[i].r_endend = 0;
	
	/* prepare the new current and bitmap and swap it with the old ones*/
	current_tmp = i << NRRD_LENG;
	memset(wr_slt->old_bitmap, 0, ((reg->readers) / sizeof(char) + 1) );
	current_tmp = __sync_lock_test_and_set(&reg->current, current_tmp);
	wr_slt->old_bitmap = __sync_lock_test_and_set(&reg->current_reader, wr_slt->old_bitmap); // <- NON SONO SICURO SIA CORRETTO COSI: io voglio swappare degli indirizzi
	
	/* update the r_started field in the old slot or free the memory used */
	reg->rw_space[current_index].r_started = (current_tmp & NRRD_MASK);
	
	return reg->rw_space[i].value_loc; //forse per correttezza poi sarà meglio toglierlo
}


/**
* Read the value of the register
*
* @author Mauro Ianni
* @param the pointer to the reader register slot, the address of the value to write and it's size
* @return the pointer to a memory location with a copy of the current value
* @date 10/03/2016
*/
void *reg_read(struct reader_slot *rd_slt){
	unsigned long long curr_tmp, index;
	unsigned int me, size;
	void *mem_loc;
	struct wf_register * reg;
	
	me = rd_slt->id;
	reg = rd_slt->parent;
	
	index = reg->current >> NRRD_LENG; //<- se non serve controllare il current, si può togliere
	
	/* if the value is not changed from the last read, the function return the local copy of the reader */
	if(rd_slt->current == index && ((reg->current_reader[me/8]) & (1 << me % 8)) ){ //<-non so se serve ancora la verifica sul current
		return rd_slt->value_loc;
	}
	
	/* get the current index and atomically increment the counter associated */
	curr_tmp = (unsigned long long) __sync_add_and_fetch(&(reg->current), 1); ///la lettura del current ed il suo incremento così sono separati!!!!
	index = curr_tmp >> NRRD_LENG;
	size = reg->rw_space[index].size;
	/* if necessary, allocate new memory to save the value for the single reader */
	if(rd_slt->size != size){
		free(rd_slt->value_loc);
		if((rd_slt->value_loc = malloc(reg->rw_space[index].size))==NULL){
			printf("malloc failed\n");
			abort();
		}
		rd_slt->size = size;
	}
	/* read from the memory location */
	mem_loc = reg->rw_space[index].value_loc;
	memcpy(rd_slt->value_loc, mem_loc, size);
	rd_slt->size = size;
	
	/* update the bitmap */
	char * bm = reg->current_reader;
	__sync_fetch_and_or(&(bm[me/8]),(1 << me % 8)); //qui sono sicuro che sono l'unico a lavorare su quel bit
	if(reg->current >> NRRD_LENG != index){ // in this way I'm sure that, when I have setted the bitmap, the current was the same
		__sync_fetch_and_and(&(bm[me/8]),~(1 << me % 8));
	}
	
	/* increment the counter of the ended reads and, if possible, free the memory */
	__sync_add_and_fetch(&(reg->rw_space[index].r_endend), 1);
	
	return rd_slt->value_loc;
}

/**
* get the size of the current value read
*
* @author Mauro Ianni
* @param the pointer to the reader register slot
* @return the size of the register as an unsigne int
* @date 10/03/2016
*/
unsigned int get_size(struct reader_slot *rd_slt){
	return rd_slt->size;
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


