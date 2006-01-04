#ifndef CC_H
#define CC_H

#define CC_VBIBUFSIZE 65536*2

//cc is 32 columns per row, this allows for extra characters
#define CC_BUFSIZE 256

struct cc
{
    int fd;
    char buffer[CC_VBIBUFSIZE];
    int code1;
    int code2;

    int samples_per_line;
    int start_line;
    int line_count;
};

int cc_decode(unsigned char *vbiline);
void cc_decode(struct cc *cc);

struct cc *cc_open(const char *vbi_name);
void cc_close(struct cc *cc);
void cc_handler(struct cc *cc);

#endif

