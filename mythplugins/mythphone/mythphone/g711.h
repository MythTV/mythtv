#ifndef G711_H_
#define G711_H_

#include <qsocketdevice.h>
#include "rtp.h"

extern "C" {
#include "gsm/gsm.h"
}

extern uchar alaw_comp_table[16384];
extern uchar ulaw_comp_table[16384];
extern short alaw_exp_table[256];
extern short ulaw_exp_table[256];

class g711alaw : public codec
{
public:
    g711alaw() :codec() {};
    ~g711alaw() {};
    virtual int Decode(uchar *In, short *out, int Len, short &maxPower);
    virtual int Encode(short *In, uchar *out, int Samples, short &maxPower, int gain);
    virtual int Silence(uchar *out, int ms);
    virtual QString WhoAreYou() { return "G711a"; }
    virtual int bandwidth()     { return 106; } // 106kbps including PPP and ATM overheads
private:
};

class g711ulaw : public codec
{
public:
    g711ulaw() :codec() {};
    ~g711ulaw() {};
    virtual int Decode(uchar *In, short *out, int Len, short &maxPower);
    virtual int Encode(short *In, uchar *out, int Samples, short &maxPower, int gain);
    virtual int Silence(uchar *out, int ms);
    virtual QString WhoAreYou() { return "G711u"; };
    virtual int bandwidth()     { return 106; } // 106kbps including PPP and ATM overheads
private:
};

class gsmCodec : public codec
{
public:
    gsmCodec();
    ~gsmCodec();
    virtual int Decode(uchar *In, short *out, int Len, short &maxPower);
    virtual int Encode(short *In, uchar *out, int Samples, short &maxPower, int gain);
    virtual int Silence(uchar *out, int ms);
    virtual QString WhoAreYou() { return "GSM"; };
    virtual int bandwidth()     { return 43; } // 43kbps including PPP and ATM overheads
private:
    gsm gsmEncData;
    gsm gsmDecData;
    bool gsmMicrosoftCompatability;
};

#ifdef VA_G729
class g729 : public codec
{
public:
    g729();
    ~g729() {};
    virtual int Decode(uchar *In, short *out, int Len, short &maxPower);
    virtual int Encode(short *In, uchar *out, int Samples, short &maxPower, int gain);
    virtual int Silence(uchar *out, int ms);
    virtual QString WhoAreYou() { return "G729"; };
    virtual int bandwidth()     { return 43; } // 43kbps including PPP and ATM overheads
private:
};
#endif

#endif
