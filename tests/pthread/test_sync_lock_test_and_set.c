#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <emscripten/threading.h>
#include <emscripten.h>
#include <assert.h>
#include <stdint.h>

//#define TYPE uint32_t
//#define PRINT_FORMAT "%u"
#define ATOMIC_EXCHANGE __sync_lock_test_and_set
#define ATOMIC_STORE emscripten_atomic_store_u32

#define TYPE uint64_t
#define PRINT_FORMAT "%llu"
#define ATOMIC_EXCHANGE emscripten_atomic_exchange_u64
#define ATOMIC_STORE emscripten_atomic_store_u64

#define N 10
TYPE array[N] = {};

void *swapper(void *arg)
{
	for(int times = 0; times < 100000; ++times)
	{
		int i = rand()%N;

		TYPE removedI;
		do { // Remove item i from array:
			removedI = ATOMIC_EXCHANGE(array+i, -1);
		} while(removedI == -1);

		// Swap pairs i<->j using atomic exchanges:

		int j;
		TYPE removedJ;
		do { // Remove item j from array:
			j = rand()%N;
			removedJ = ATOMIC_EXCHANGE(array+j, -1);
		} while(removedJ == -1);

		// Put the items back, in each others places:
		ATOMIC_STORE(array+i, removedJ);
		ATOMIC_STORE(array+j, removedI);
	}
	return 0;
}

int compare_function(const void *a,const void *b)
{
	return *(int *)a - *(int *)b;
}

int main()
{
	srand(time(NULL));

	printf("Initialize\n");
	for(int i = 0; i < N; ++i)
		array[i] = i;

#define T 4
	pthread_t threads[T];

	printf("Launch threads\n");
	for(int i = 0; i < T; ++i)
	{
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		int rc = pthread_create(&threads[i], &attr, swapper, NULL);
		assert(rc == 0);
	}

	printf("Wait for threads to finish\n");
	for(int i = 0; i < T; ++i)
	{
		int rc = pthread_join(threads[i], NULL);
		assert(rc == 0);
	}

	emscripten_current_thread_process_queued_calls();

	printf("Test the integrity of the results\n");

	int allInPlace = 1;
	for(int i = 0; i < N; ++i)
	{
		printf("%d: " PRINT_FORMAT "\n", i, array[i]);
		if (array[i] != i)
			allInPlace = 0;
	}

	assert(!allInPlace);

	qsort(array, N, sizeof(TYPE), compare_function);

	for(int i = 0; i < N; ++i)
	{
		printf("%d: " PRINT_FORMAT "\n", i, array[i]);
		assert(array[i] == i);
	}

	printf("All done!\n");
	EM_ASM(Module['noExitRuntime']=1);
}
