#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include "jody_hash.h"

#define BLOCKSIZE 4096

int main(int argc, char **argv)
{
	static struct timeval starttime, endtime;
	static int64_t elapsed;
	static hash_t hash = 0;
	static unsigned long iterations, cnt;
	static char block[BLOCKSIZE];

	if (argc != 2) {
		fprintf(stderr, "Specify number of iterations to run\n");
		exit(EXIT_FAILURE);
	}

	iterations = atoi(argv[1]);

	if (iterations < 1) {
		fprintf(stderr, "Iteration count must be a positive integer\n");
		exit(EXIT_FAILURE);
	}

	gettimeofday(&starttime, NULL);
	for (cnt = iterations; cnt; cnt--) hash = jody_block_hash((hash_t *)&block, hash, BLOCKSIZE);
	gettimeofday(&endtime, NULL);
	elapsed = endtime.tv_sec - starttime.tv_sec;
	elapsed *= 1000000;
	elapsed += (endtime.tv_usec - starttime.tv_usec);

	printf("%lu blocks in %lu uSec (%lu blocks per second, %lu MB/sec overall)\n", iterations, elapsed,
			(unsigned long)((iterations * 1000000) / elapsed),
			(unsigned long)((iterations * 1000000) / elapsed) * BLOCKSIZE / 1048576
			);
	exit(EXIT_SUCCESS);
}
