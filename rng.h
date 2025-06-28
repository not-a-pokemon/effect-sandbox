#pragma once
#include <stdint.h>

typedef struct rng_state_s {
	uint8_t table[256];
	int index;
} rng_state_s;

void rng_init(rng_state_s *rng);
uint32_t rng_next(rng_state_s *rng);
