#ifndef CHANNEL_H
#define CHANNEL_H

#include <map>
#include <qstring.h>
#include "channelbase.h"
#include "videodev_myth.h" // needed for v4l2_std_id type

using namespace std;

#define FAKE_VIDEO 0

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
    unsigned short *GetV4L1Field(int attrib, 
                                          struct video_picture &vid_pic);
    int ChangeColourAttribute(int attrib, const char *name, bool up);
    int ChangeColour(bool up);
    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
    int ChangeHue(bool up);
    void SetColourAttribute(int attrib, const char *name);
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

    int videomode_v4l1;
    v4l2_std_id videomode_v4l2;
    bool usingv4l2;

    QString currentFormat;

    int defaultFreqTable;

    bool commfree;

    bool usingATSCstrength;
};

#endif
