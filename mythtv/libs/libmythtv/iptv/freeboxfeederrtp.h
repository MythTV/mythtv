/** -*- Mode: c++ -*-
 *  FreeboxFeederRtp
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FREEBOXFEEDERRTP_H
#define FREEBOXFEEDERRTP_H

// MythTV headers
#include "freeboxfeederlive.h"

class SimpleRTPSource;
class FreeboxMediaSink;


class FreeboxFeederRtp : public FreeboxFeederLive
{
  public:
    FreeboxFeederRtp();
    virtual ~FreeboxFeederRtp();

    bool CanHandle(const QString &url) const;
    bool Open(const QString &url);
    bool IsOpen(void) const { return _source; }
    void Close(void);

    void AddListener(IPTVListener*);
    void RemoveListener(IPTVListener*);

  private:
    SimpleRTPSource    *_source;
    FreeboxMediaSink   *_sink;

  private:
    /// avoid default copy operator
    FreeboxFeederRtp& operator=(const FreeboxFeederRtp&);
    /// avoid default copy constructor
    FreeboxFeederRtp(const FreeboxFeederRtp&);
};

#endif//FREEBOXFEEDERRTP_H
