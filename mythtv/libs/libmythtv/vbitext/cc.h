#ifndef CC_H
#define CC_H

#include <qstring.h>

#define CC_VBIBUFSIZE 65536

//cc is 32 columns per row, this allows for extra characters
#define CC_BUFSIZE 256

struct cc
{
    int fd;
    char buffer[CC_VBIBUFSIZE];
    int code1;
    int code2;

    // per-field
    int badvbi[2];
    int lasttc[2];
    int lastcode[2];
    int lastcodetc[2];
    int ccmode[2];	// 0=cc1/txt1, 1=cc2/txt2
    int txtmode[4];
    int xds[2];

    // per-mode state
    int lastrow[8];
    int newrow[8];
    int newcol[8];
    int timecode[8];
    int row[8];
    int col[8];
    int rowcount[8];
    int style[8];
    int linecont[8];
    int resumetext[8];
    int lastclr[8];
    QString ccbuf[8];
};

int cc_decode(unsigned char *vbiline);

struct cc *cc_open(const char *vbi_name);
void cc_close(struct cc *cc);
void cc_handler(struct cc *cc);

#endif

