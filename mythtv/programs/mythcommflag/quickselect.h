#ifndef QUICKSELECT_H
#define QUICKSELECT_H

unsigned char quick_select(unsigned char *arr, int nelems, int select);
unsigned char quick_select_median(unsigned char *arr, int nelems);

unsigned short quick_select_ushort(unsigned short *arr, int nelems, int select);
unsigned short quick_select_median_ushort(unsigned short *arr, int nelems);

float quick_select_float(float *arr, int nelems, int select);
float quick_select_median_float(float *arr, int nelems);

#endif  /* !QUICKSELECT_H */
