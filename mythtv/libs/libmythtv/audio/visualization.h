// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef LIBMYTHTV_AUDIO_VISUALIZATION_H
#define LIBMYTHTV_AUDIO_VISUALIZATION_H

#include <chrono>

#include <QMutex>

class Visualization
{
  public:
    Visualization() = default;
    virtual ~Visualization() = default;

    virtual void add(const void *b, unsigned long b_len,
                     std::chrono::milliseconds timecode, int c, int p) = 0;
    virtual void prepare() = 0;

    QMutex *mutex() { return &m_mtx; }

  private:
    QMutex m_mtx;
};

#endif // LIBMYTHTV_AUDIO_VISUALIZATION_H
