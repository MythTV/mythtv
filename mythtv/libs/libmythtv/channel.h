#ifndef CHANNEL_H
#define CHANNEL_H

#include <map>
#include <qstring.h>
#include "frequencies.h"

using namespace std;

class TV;

class Channel
{
 public:
    Channel(TV *parent, const QString &videodevice);
   ~Channel(void);

    bool Open(void);
    void Close(void);

    void SetChannelOrdering(QString chanorder) { channelorder = chanorder; }

    void SetFormat(const QString &format);
    void SetFreqTable(const QString &name);

    bool SetChannelByString(const QString &chan); 
    bool ChannelUp(void);
    bool ChannelDown(void);
 
    void ToggleInputs(void); 
    void SwitchToInput(const QString &input);
 
    QString GetCurrentName(void);
    QString GetCurrentInput(void);

    void SetFd(int fd) { videofd = fd; } 
    QString GetDevice() { return device; }

    QString GetOrdering() { return channelorder; }

  private:
    int GetCurrentChannelNum(const QString &channame);
 
    QString device;
    bool isopen;  
    int videofd;
    QString curchannelname;

    struct CHANLIST *curList;  
    int totalChannels;

    TV *pParent;

    int videomode;

    int capchannels;
    int currentcapchannel;
    map<int, QString> channelnames;

    QString channelorder;
};

#endif
