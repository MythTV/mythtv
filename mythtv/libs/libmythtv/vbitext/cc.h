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
    int badvbi;
    int lasttc;
    int lastcode;
    int lastcodetc;
    int ccmode;	// 0=cc1/txt1, 1=cc2/txt2
    int txtmode[2];

    // per-mode state
    int lastrow[4];
    int newrow[4];
    int newcol[4];
    int timecode[4];
    int row[4];
    int col[4];
    int rowcount[4];
    int style[4];
    int linecont[4];
    int resumetext[4];
    QString ccbuf[4];
};

int cc_decode(unsigned char *vbiline);

struct cc *cc_open(const char *vbi_name);
void cc_close(struct cc *cc);
int cc_handler(struct cc *cc);

#endif

