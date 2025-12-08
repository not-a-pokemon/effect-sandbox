#ifndef _efsa_RNG_H
#define _efsa_RNG_H

#include <stdint.h>

typedef struct rng_state_s {
	uint8_t table[256];
	int index;
	int index1;
	int index2;
	int index3;
} rng_state_s;

void rng_init(rng_state_s *dice);
uint32_t rng_next(rng_state_s *dice);
uint64_t rng_bigrange(rng_state_s *dice);

#endif
