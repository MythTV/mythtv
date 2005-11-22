#ifndef CC_H
#define CC_H

#define CC_VBIBUFSIZE 65536

//cc is 32 columns per row, this allows for extra characters
#define CC_BUFSIZE 256

struct cc
{
    int fd;
    char buffer[CC_VBIBUFSIZE];
    int code1;
    int code2;
};

int cc_decode(unsigned char *vbiline);

struct cc *cc_open(const char *vbi_name);
void cc_close(struct cc *cc);
void cc_handler(struct cc *cc);

#endif

