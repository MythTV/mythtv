#ifndef TV_H
#define TV_H

#include <mysql.h>

#include "NuppelVideoRecorder.h"
#include "NuppelVideoPlayer.h"
#include "RingBuffer.h"
#include "settings.h"

#include "channel.h"

class TV
{
 public:
    TV(void);
   ~TV(void);

    void LiveTV(void);

    bool CheckChannel(int channum);

 private:
    void ChangeChannel(bool up);
    void ChangeChannel(char *name);
    
    void ChannelKey(int key);
    void ChannelCommit(void);
   
    void GetChannelInfo(int lchannel, string &title, string &subtitle, 
                        string &desc, string &category, string &starttime,
                        string &endtime);

    void ConnectDB(void);
    void DisconnectDB(void);

    NuppelVideoRecorder *nvr;
    NuppelVideoPlayer *nvp;

    RingBuffer *rbuffer;
    Channel *channel;

    Settings *settings;

    int osd_display_time;

    bool channelqueued;
    char channelKeys[4];
    int channelkeysstored;

    MYSQL *db_conn;
};

#endif
