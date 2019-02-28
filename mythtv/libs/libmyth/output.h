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

#include <QMutex>
#include <QList>

#include "mythobservable.h"
#include "mythexp.h"

class QObject;
class Buffer;

namespace MythTV {
class Visual;
}

class MPUBLIC OutputEvent : public MythEvent
{
  public:
    explicit OutputEvent(Type type) :
        MythEvent(type) {}
    OutputEvent(long s, unsigned long w, int b, int f, int p, int c) :
        MythEvent(Info), m_elasped_seconds(s), m_written_bytes(w),
        m_brate(b), m_freq(f), m_prec(p), m_chan(c) {}
    explicit OutputEvent(const QString &e) :
        MythEvent(Error)
    {
        QByteArray tmp = e.toUtf8();
        m_error_msg = new QString(tmp.constData());
    }

    ~OutputEvent()
    {
        delete m_error_msg;
    }

    const QString *errorMessage() const { return m_error_msg; }

    const long &elapsedSeconds() const { return m_elasped_seconds; }
    const unsigned long &writtenBytes() const { return m_written_bytes; }
    const int &bitrate() const { return m_brate; }
    const int &frequency() const { return m_freq; }
    const int &precision() const { return m_prec; }
    const int &channels() const { return m_chan; }

    MythEvent *clone(void) const override // MythEvent
        { return new OutputEvent(*this); }

    static Type Playing;
    static Type Buffering;
    static Type Info;
    static Type Paused;
    static Type Stopped;
    static Type Error;

  private:
    OutputEvent(const OutputEvent &o) : MythEvent(o),
        m_elasped_seconds(o.m_elasped_seconds),
        m_written_bytes(o.m_written_bytes),
        m_brate(o.m_brate), m_freq(o.m_freq),
        m_prec(o.m_prec), m_chan(o.m_chan)
    {
        if (o.m_error_msg)
        {
            m_error_msg = new QString(*o.m_error_msg);
        }
    }
    OutputEvent &operator=(const OutputEvent&);

  private:
    QString       *m_error_msg       {nullptr};

    long           m_elasped_seconds {0};
    unsigned long  m_written_bytes   {0};
    int            m_brate           {0};
    int            m_freq            {0};
    int            m_prec            {0};
    int            m_chan            {0};
};

typedef std::vector<MythTV::Visual*> Visuals;

class MPUBLIC OutputListeners : public MythObservable
{
public:
    OutputListeners() = default;
    virtual ~OutputListeners() = default;

    bool hasVisual(void) { return m_visuals.size(); }
    void addVisual(MythTV::Visual *);
    void removeVisual(MythTV::Visual *);

    QMutex *mutex() { return &m_mtx; }

    void setBufferSize(unsigned int sz) { m_bufsize = sz; }
    unsigned int bufferSize() const { return m_bufsize; }

protected:
    void error(const QString &e);
    void dispatchVisual(uchar *b, unsigned long b_len,
                       unsigned long written, int chan, int prec);
    void prepareVisuals();

private:
    Q_DISABLE_COPY(OutputListeners)

    QMutex       m_mtx;
    Visuals      m_visuals;

    unsigned int m_bufsize {0};
};


#endif // __output_h
