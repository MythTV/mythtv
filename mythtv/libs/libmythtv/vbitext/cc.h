#ifndef CC_H
#define CC_H

#define CC_VBIBUFSIZE 65536

//cc is 32 columns per row, this allows for extra characters
#define CC_BUFSIZE 256

struct cc
{
    int fd;
    char buffer[CC_VBIBUFSIZE];
    int lastcode;
    int ccmode;	// 0=cc1 or 1=cc2
    char ccbuf[2][CC_BUFSIZE];
};

int cc_decode(unsigned char *vbiline);

struct cc *cc_open(const char *vbi_name);
void cc_close(struct cc *cc);
int cc_handler(struct cc *cc);

#endif

