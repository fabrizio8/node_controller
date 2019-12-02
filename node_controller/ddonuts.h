/** ddonuts.h include for distributed process implementation **/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <stdlib.h>

#define		SEMKEY	(key_t)5763 // use 9 digit value
#define		MEMKEY	(key_t)5763 // use 9 digit value
#define		NUMFLAVORS	 4
#define		NUMSLOTS         900
#define		NUMSEMIDS	 2
#define		PROD		 0
#define		CONSUMER	 1

typedef struct a_donut{
 int   node_id;
 int   prod_id;
 int   donut_num;
} DONUT;

struct	donut_ring{
	DONUT	flavor[NUMFLAVORS][NUMSLOTS];
	int	outptr[NUMFLAVORS];
	int	in_ptr[NUMFLAVORS];
	int	serial[NUMFLAVORS];
};


int	p(int, int);
int	v(int, int);
int	semsetall(int, int, int);
