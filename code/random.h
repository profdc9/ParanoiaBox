#ifndef _RANDOM_H
#define _RANDOM_H

/*
 * Copyright (c) 2018 Daniel Marks

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define RNG_APP_TAG "ParanoiaBox"

void random_initialize(void);
void randomness_test(void);
void randomness_show(void);
int random_circuit_check(void);
void randomness_get_raw_random_bits(uint8_t randomdata[], int bytes);
void randomness_get_whitened_bits(uint8_t whitenedbytes[], size_t bytes);
void random_stir_in_entropy(void);

#ifdef __cplusplus
}
#endif

#endif  /* _RANDOM_H */
