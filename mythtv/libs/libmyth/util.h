#ifndef UTIL_H_
#define UTIL_H_

#include <qsocket.h>
#include <qstringlist.h>
#include <qcolor.h>

class QPixmap;
class QImage;
class QPainter;

bool WriteStringList(QSocket *socket, QStringList &list);
bool ReadStringList(QSocket *socket, QStringList &list);

void WriteBlock(QSocket *socket, void *data, int len);
int ReadBlock(QSocket *socket, void *data, int maxlen);

void encodeLongLong(QStringList &list, long long num);
long long decodeLongLong(QStringList &list, int offset);

#ifndef QWS
void GetMythTVGeometry(Display *dpy, int screen_num, int *x, int *y, 
                       int *w, int *h);
#endif

QRgb blendColors(QRgb source, QRgb add, int alpha);

void blendImageToPixmap(QPixmap *source, int sourcex, int sourcey,
                        QImage *image, QPainter *destination, int destx,
                        int desty);

#endif
