#include "sdtxt.h"

#include <string.h>

// the universe is bound in equal parts by arrogance and altruism, any attempt to alter this would be suicide

int _sd2txt_len(const char *key, char *val)
{
    int ret = strlen(key);
    if(!*val) return ret;
    ret += strlen(val);
    ret++;
    return ret;
}

void _sd2txt_count(xht h, const char *key, void *val, void *arg)
{
    int *count = (int*)arg;
    *count += _sd2txt_len(key,(char*)val) + 1;
}

void _sd2txt_write(xht h, const char *key, void *val, void *arg)
{
    unsigned char **txtp = (unsigned char **)arg;
    char *cval = (char*)val;
    int len;

    // copy in lengths, then strings
    **txtp = _sd2txt_len(key,(char*)val);
    (*txtp)++;
    memcpy(*txtp,key,strlen(key));
    *txtp += strlen(key);
    if(!*cval) return;
    **txtp = '=';
    (*txtp)++;
    memcpy(*txtp,cval,strlen(cval));
    *txtp += strlen(cval);
}

unsigned char *sd2txt(xht h, int *len)
{
    unsigned char *buf, *raw;
    *len = 0;

    xht_walk(h,_sd2txt_count,(void*)len);
    if(!*len)
    {
        *len = 1;
        buf = (unsigned char *)malloc(1);
        *buf = 0;
        return buf;
    }
    raw = buf = (unsigned char *)malloc(*len);
    xht_walk(h,_sd2txt_write,&buf);
    return raw;
}

xht txt2sd(unsigned char *txt, int len)
{
    char key[256], *val;
    xht h = 0;

    if(txt == 0 || len == 0 || *txt == 0) return 0;
    h = xht_new(23);

    // loop through data breaking out each block, storing into hashtable
    for(;*txt <= len && len > 0; len -= *txt, txt += *txt + 1)
    {
        if(*txt == 0) break;
        memcpy(key,txt+1,*txt);
        key[*txt] = 0;
        if((val = strchr(key,'=')) != 0)
        {
            *val = 0;
            val++;
        }
        xht_store(h, key, strlen(key), val, strlen(val));
    }
    return h;
}
