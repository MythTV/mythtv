#ifndef HAMM_H
#define HAMM_H

#ifdef __cplusplus
extern "C" {
#endif

int hamm8(const unsigned char *p, int *err);
int hamm16(const unsigned char *p, int *err);
int hamm24(const unsigned char *p, int *err);
int chk_parity(unsigned char *p, int n);

#ifdef __cplusplus
}
#endif

#endif

