#ifndef CHANNEL_H
#define CHANNEL_H

#include <map>
#include <qstring.h>
#include "frequencies.h"

using namespace std;

class TVRec;

class Channel
{
 public:
    Channel(TVRec *parent, const QString &videodevice);
   ~Channel(void);

    bool Open(void);
    void Close(void);

    void SetChannelOrdering(QString chanorder) { channelorder = chanorder; }

    void SetFormat(const QString &format);
    void SetFreqTable(const QString &name);

    bool SetChannelByString(const QString &chan); 
    bool ChannelUp(void);
    bool ChannelDown(void);
    bool NextFavorite(void);

    int ChangeColour(bool up);
    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
 
    void SetContrast();
    void SetBrightness();
    void SetColour();

    void ToggleInputs(void); 
    void SwitchToInput(const QString &input);
    void SwitchToInput(const QString &input, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting);

    int GetInputByName(const QString &input);
    QString GetInputByNum(int capchannel);

    void StoreInputChannels(void);
 
    QString GetCurrentName(void);
    QString GetCurrentInput(void);

    void SetFd(int fd) { videofd = fd; } 
    QString GetDevice() { return device; }

    QString GetOrdering() { return channelorder; }

  private:
    int GetCurrentChannelNum(const QString &channame);

    bool TuneTo(const QString &chan, int finetune);
    bool ChangeExternalChannel(const QString &newchan);

    QString device;
    bool isopen;  
    int videofd;
    QString curchannelname;

    struct CHANLIST *curList;  
    int totalChannels;

    TVRec *pParent;

    int videomode;

    int capchannels;
    int currentcapchannel;
    map<int, QString> channelnames;
    map<int, QString> inputTuneTo;
    map<int, QString> externalChanger;
    map<int, QString> inputChannel;

    QString channelorder;

    bool usingv4l2;
};

#endif
