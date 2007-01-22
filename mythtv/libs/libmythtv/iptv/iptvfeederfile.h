/** -*- Mode: c++ -*-
 *  IPTVFeederFile
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_FEEDER_FILE_H_
#define _IPTV_FEEDER_FILE_H_

// MythTV headers
#include "iptvfeederlive.h"

class TSDataListener;
class ByteStreamFileSource;
class IPTVMediaSink;


class IPTVFeederFile : public IPTVFeederLive
{
  public:
    IPTVFeederFile();
    virtual ~IPTVFeederFile();

    bool CanHandle(const QString &url) const { return IsFile(url); }
    bool IsOpen(void) const { return _source; }

    bool Open(const QString &url);
    void Close(void);

    void AddListener(TSDataListener*);
    void RemoveListener(TSDataListener*);

    static bool IsFile(const QString &url);

  private:
    IPTVFeederFile &operator=(const IPTVFeederFile&);
    IPTVFeederFile(const IPTVFeederFile&);

  private:
    ByteStreamFileSource *_source;
    IPTVMediaSink        *_sink;
};

#endif //_IPTV_FEEDER_FILE_H_
