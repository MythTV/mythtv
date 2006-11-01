/** -*- Mode: c++ -*
 *  FreeboxFeederFile
 *  Copyright (c) 2006 by Mike Mironov & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FREEBOXFEEDERFILE_H
#define FREEBOXFEEDERFILE_H

// MythTV headers
#include "freeboxfeederlive.h"

class IPTVListener;
class ByteStreamFileSource;
class FreeboxMediaSink;


class FreeboxFeederFile : public FreeboxFeederLive
{
  public:
    FreeboxFeederFile();
    virtual ~FreeboxFeederFile();

    bool CanHandle(const QString &url) const;
    bool Open(const QString &url);
    bool IsOpen(void) const { return _source; }
    void Close(void);

    void AddListener(IPTVListener*);
    void RemoveListener(IPTVListener*);

  private:
    ByteStreamFileSource    *_source;
    FreeboxMediaSink        *_sink;

  private:
    /// avoid default copy operator
    FreeboxFeederFile &operator=(const FreeboxFeederFile&);
    /// avoid default copy constructor
    FreeboxFeederFile(const FreeboxFeederFile&);
};

#endif//FREEBOXFEEDERFILE_H
