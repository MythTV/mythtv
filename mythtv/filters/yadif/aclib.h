#ifndef _ACLIB_H_
#define _ACLIB_H_

void * fast_memcpy_SSE(void * to, const void * from, size_t len);
void * fast_memcpy_MMX(void * to, const void * from, size_t len);
void * fast_memcpy_MMX2(void * to, const void * from, size_t len);
void * fast_memcpy_3DNow(void * to, const void * from, size_t len);

#endif //_ACLIB_H_

