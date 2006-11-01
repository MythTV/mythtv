/** -*- Mode: c++ -*-
 *  IPTVListener
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_LISTENER_H_
#define _IPTV_LISTENER_H_

class IPTVListener
{
  public:
    /// Callback function to add MPEG2 TS data
    virtual void AddData(unsigned char *data,
                         unsigned int   dataSize) = 0;

  protected:
    virtual ~IPTVListener() {}
};

#endif // _IPTV_LISTENER_H_
