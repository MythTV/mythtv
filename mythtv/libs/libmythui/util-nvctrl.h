#ifndef __UTIL_NVIDIA_H__
#define __UTIL_NVIDIA_H__

#include <cstdlib>
#include <map>
#include <vector>
using namespace std;

typedef map<unsigned int, double> t_screenrate;

int GetNvidiaRates(t_screenrate& screenmap);
int CheckNVOpenGLSyncToVBlank(void);

#endif
