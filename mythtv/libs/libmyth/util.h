#ifndef UTIL_H_
#define UTIL_H_

#include <qsocket.h>
#include <qstringlist.h>

bool WriteStringList(QSocket *socket, QStringList &list);
bool ReadStringList(QSocket *socket, QStringList &list);

void WriteBlock(QSocket *socket, void *data, int len);
int ReadBlock(QSocket *socket, void *data, int maxlen);

void encodeLongLong(QStringList &list, long long num);
long long decodeLongLong(QStringList &list, int offset);

void GetMythTVGeometry(Display *dpy, int screen_num, int *x, int *y, int *w, 
                       int *h);

#endif
