void __VERIFIER_error() __attribute__ ((__noreturn__));

void __VERIFIER_error()
{
	while ( 1 )
		;
}

#include <pthread.h>

unsigned int i=1, j=1;

#define NUM 5

void *t1 ( void* arg )
{
	unsigned int k = 0;

	for (k = 0; k < NUM; k++)
		i+=j;

	return NULL;
}

void * t2 ( void* arg )
{
	unsigned int k = 0;

	for (k = 0; k < NUM; k++)
		j+=i;

	return NULL;
}

int main ( int argc, char **argv )
{
	pthread_t id1, id2;
	pthread_create ( &id1, NULL, t1, NULL );
	pthread_create ( &id2, NULL, t2, NULL );

	if (i >= 144 || j >= 144) {
ERROR: __VERIFIER_error();
	}

	return 0;
}
