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

    void SetFormat(const QString &format);
    void SetFreqTable(const QString &name);

    bool SetChannelByString(const QString &chan); 
    bool SetChannel(int i);
    bool ChannelUp(void);
    bool ChannelDown(void);
 
    void ToggleInputs(void); 
    void SwitchToInput(const QString &input);
 
    QString GetCurrentName(void);
    QString GetCurrentInput(void);

    int GetCurrent(void) { return curchannel + 1; }
    
    void SetFd(int fd) { videofd = fd; } 
    QString GetDevice() { return device; }

  private:
    
    QString device;
    bool isopen;  
    int videofd;
    int curchannel;

    struct CHANLIST *curList;  
    int totalChannels;

    TV *pParent;

    int videomode;

    int capchannels;
    int currentcapchannel;
    map<int, QString> channelnames;
};

#endif
