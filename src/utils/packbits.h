#ifndef __PACKBITS_H
#define __PACKBITS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int packbits(int8_t* source, int8_t* dest, int size);
int8_t* unpackbits(int8_t* sourcep, int8_t* destp, int destcount);

#ifdef __cplusplus
}
#endif

#endif