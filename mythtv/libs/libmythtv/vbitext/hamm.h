#ifndef HAMM_H
#define HAMM_H

int hamm8(unsigned char *p, int *err);
int hamm16(unsigned char *p, int *err);
int hamm24(unsigned char *p, int *err);
int chk_parity(unsigned char *p, int n);

#endif

