#ifndef __PACKBITS_H
#define __PACKBITS_H

#ifdef __cplusplus
extern "C" {
#endif

int packbits(char* source, char* dest, int size);
char* unpackbits(char* sourcep, char* destp, int destcount);

#ifdef __cplusplus
}
#endif

#endif