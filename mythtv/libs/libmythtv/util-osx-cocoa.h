#ifndef _UTIL_OSX_H__
#define _UTIL_OSX_H__

#ifdef USING_DVDV
void *CreateOSXCocoaPool(void);
void DeleteOSXCocoaPool(void*&);
#else
inline void *CreateOSXCocoaPool(void) { return NULL; }
inline void DeleteOSXCocoaPool(void*&) {}
#endif

#endif // _UTIL_OSX_H__
