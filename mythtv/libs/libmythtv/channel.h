#ifndef CHANNEL_H
#define CHANNEL_H

#include <map>
#include <qstring.h>
#include "channelbase.h"

using namespace std;

class TVRec;

// Implements tuning for analog TV cards (both software and hardware encoding)
// using the V4L driver API
class Channel : public ChannelBase
{
 public:
    Channel(TVRec *parent, const QString &videodevice, bool strength = false);
    virtual ~Channel(void);

    bool Open(void);
    void Close(void);

    void SetFormat(const QString &format);
    int SetDefaultFreqTable(const QString &name);

    bool SetChannelByString(const QString &chan); 
    bool ChannelUp(void);
    bool ChannelDown(void);

    bool CheckSignalFull(void);
    bool CheckSignal(int msecWait = 5000, 
                     int requiredSignalPercentage = 65,
                     int input = 0);
    unsigned short *Channel::GetV4L1Field(int attrib, 
                                          struct video_picture &vid_pic);
    int Channel::ChangeColourAttribute(int attrib, const char *name, bool up);
    int ChangeColour(bool up);
    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
    int ChangeHue(bool up);
    void Channel::SetColourAttribute(int attrib, const char *name);
    void SetContrast();
    void SetBrightness();
    void SetColour();
    void SetHue();

    void SwitchToInput(int newcapchannel, bool setstarting);

    void SetFd(int fd); 

    bool IsCommercialFree() { return commfree; }

  private:
    int GetCurrentChannelNum(const QString &channame);

    void SetFreqTable(const int index);
    int SetFreqTable(const QString &name);
    bool TuneTo(const QString &chan, int finetune);
    bool TuneToFrequency(int frequency);

    QString device;
    bool isopen;  
    int videofd;

    struct CHANLIST *curList;  
    int totalChannels;

    int videomode;
    bool usingv4l2;

    QString currentFormat;

    int defaultFreqTable;

    bool commfree;

    bool usingATSCstrength;
};

#endif
