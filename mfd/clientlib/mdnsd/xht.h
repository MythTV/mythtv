#ifndef xht_h
#define xht_h

// simple string->void* hashtable, very static and bare minimal, but efficient

typedef struct xht_struct *xht;

// must pass a prime#
xht xht_new(int prime);

// caller responsible for key storage, no copies made (don't free it b4 xht_free()!)
// set val to NULL to clear an entry, memory is reused but never free'd (# of keys only grows to peak usage)
void xht_set(xht h, const char *key, void *val);

// ooh! unlike set where key/val is in caller's mem, here they are copied into xht and free'd when val is 0 or xht_free()
void xht_store(xht h, const char *key, int klen, void *val, int vlen);

// returns value of val if found, or NULL
void *xht_get(xht h, const char *key);

// free the hashtable and all entries
void xht_free(xht h);

// pass a function that is called for every key that has a value set
typedef void (*xht_walker)(xht h, const char *key, void *val, void *arg);
void xht_walk(xht h, xht_walker w, void *arg);

#endif

