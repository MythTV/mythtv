#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include "frequencies.h"

using namespace std;

class Channel
{
 public:
    Channel(const string &videodevice);
   ~Channel(void);

    void Open(void);
    void Close(void);

    void SetFormat(const string &format);
    void SetFreqTable(const string &name);

    bool SetChannelByString(const string &chan); 
    bool SetChannel(int i);
    bool ChannelUp(void);
    bool ChannelDown(void);
  
    char *GetCurrentName(void);
    int GetCurrent(void) { return curchannel + 1; }
    
    void SetFd(int fd) { videofd = fd; } 
 private:
    
    string device;
    bool isopen;  
    int videofd;
    int curchannel;

    struct CHANLIST *curList;  
    int totalChannels;
};

#endif
