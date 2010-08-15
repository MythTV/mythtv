// -*- Mode: c++ -*-
// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef   __output_h
#define   __output_h

class OutputListeners;
class OutputEvent;

#include <vector>
using namespace std;

#include <QMutex>
#include <QList>

#include "mythobservable.h"

class QObject;
class Buffer;

namespace MythTV {
class Visual;
}

class MPUBLIC OutputEvent : public MythEvent
{
  public:
    OutputEvent(Type t) :
        MythEvent(t), error_msg(NULL), elasped_seconds(0), written_bytes(0),
        brate(0), freq(0), prec(0), chan(0)
    { ; }

    OutputEvent(long s, unsigned long w, int b, int f, int p, int c) :
        MythEvent(Info), error_msg(NULL), elasped_seconds(s), written_bytes(w),
        brate(b), freq(f), prec(p), chan(c)
    { ; }

    OutputEvent(const QString &e) :
        MythEvent(Error), elasped_seconds(0), written_bytes(0),
        brate(0), freq(0), prec(0), chan(0)
    {
        QByteArray tmp = e.toUtf8();
        error_msg = new QString(tmp.constData());
    }

    ~OutputEvent()
    {
        delete error_msg;
    }

    const QString *errorMessage() const { return error_msg; }

    const long &elapsedSeconds() const { return elasped_seconds; }
    const unsigned long &writtenBytes() const { return written_bytes; }
    const int &bitrate() const { return brate; }
    const int &frequency() const { return freq; }
    const int &precision() const { return prec; }
    const int &channels() const { return chan; }

    virtual MythEvent *clone(void) const { return new OutputEvent(*this); }

    static Type Playing;
    static Type Buffering;
    static Type Info;
    static Type Paused;
    static Type Stopped;
    static Type Error;

  private:
    OutputEvent(const OutputEvent &o) : MythEvent(o),
        error_msg(NULL),
        elasped_seconds(o.elasped_seconds),
        written_bytes(o.written_bytes),
        brate(o.brate), freq(o.freq), prec(o.prec), chan(o.chan)
    {
        if (o.error_msg)
        {
            error_msg = new QString(*o.error_msg);
            error_msg->detach();
        }
    }
    OutputEvent &operator=(const OutputEvent&);

  private:
    QString *error_msg;

    long elasped_seconds;
    unsigned long written_bytes;
    int brate, freq, prec, chan;
};

typedef vector<MythTV::Visual*> Visuals;

class MPUBLIC OutputListeners : public MythObservable
{
public:
    OutputListeners();
    virtual ~OutputListeners();

    void addVisual(MythTV::Visual *);
    void removeVisual(MythTV::Visual *);

    QMutex *mutex() { return &mtx; }

    void setBufferSize(unsigned int sz) { bufsize = sz; }
    unsigned int bufferSize() const { return bufsize; }

protected:
    void error(const QString &e);
    void dispatchVisual(uchar *b, unsigned long b_len,
                       unsigned long written, int chan, int prec);
    void prepareVisuals();

private:
    QMutex       mtx;
    Visuals      visuals;

    unsigned int bufsize;
};


#endif // __output_h
