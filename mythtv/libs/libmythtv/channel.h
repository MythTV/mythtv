#ifndef CHANNEL_H
#define CHANNEL_H

#include <map>
#include <qstring.h>
#include "channelbase.h"

using namespace std;

class TVRec;

class Channel : public ChannelBase
{
 public:
    Channel(TVRec *parent, const QString &videodevice);
    virtual ~Channel(void);

    bool Open(void);
    void Close(void);

    void SetFormat(const QString &format);
    void SetFreqTable(const QString &name);

    bool SetChannelByString(const QString &chan); 
    bool ChannelUp(void);
    bool ChannelDown(void);

    int ChangeColour(bool up);
    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
    void SetContrast();
    void SetBrightness();
    void SetColour();

    void SwitchToInput(int newcapchannel, bool setstarting);

    void SetFd(int fd); 

  private:
    int GetCurrentChannelNum(const QString &channame);

    bool TuneTo(const QString &chan, int finetune);

    QString device;
    bool isopen;  
    int videofd;

    struct CHANLIST *curList;  
    int totalChannels;

    int videomode;
    bool usingv4l2;
};

#endif
