#ifndef _FREEBOXCHANNELFETCHER_H_
#define _FREEBOXCHANNELFETCHER_H_

#include "freeboxchannelinfo.h"

class FreeboxChannelFetcher
{
  private:
    FreeboxChannelFetcher();
    ~FreeboxChannelFetcher();

  public:
    static QString DownloadPlaylist(const QString& url);
    static fbox_chan_map_t ParsePlaylist(const QString& rawdata);
};

#endif//_FREEBOXCHANNELFETCHER_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
