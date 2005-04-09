// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef   __output_h
#define   __output_h

class OutputListeners;
class OutputEvent;

#include <qthread.h>
#include <qevent.h>
#include <qptrlist.h>

class QObject;
class Buffer;

namespace MythTV {
class Visual;
}

class OutputEvent : public QCustomEvent
{
public:
    enum Type { Playing = (User + 200), Buffering, Info, Paused,
		Stopped, Error };

    OutputEvent(Type t)
	: QCustomEvent(t), error_msg(0), elasped_seconds(0), written_bytes(0),
	  brate(0), freq(0), prec(0), chan(0)
    { ; }

    OutputEvent(long s, unsigned long w, int b, int f, int p, int c)
	: QCustomEvent(Info), error_msg(0), elasped_seconds(s), written_bytes(w),
	  brate(b), freq(f), prec(p), chan(c)
    { ; }

    OutputEvent(const QString &e)
	: QCustomEvent(Error), elasped_seconds(0), written_bytes(0),
	  brate(0), freq(0), prec(0), chan(0)
    {
	error_msg = new QString(e.utf8());
    }


    ~OutputEvent()
    {
	if (error_msg)
	    delete error_msg;
    }

    const QString *errorMessage() const { return error_msg; }

    const long &elapsedSeconds() const { return elasped_seconds; }
    const unsigned long &writtenBytes() const { return written_bytes; }
    const int &bitrate() const { return brate; }
    const int &frequency() const { return freq; }
    const int &precision() const { return prec; }
    const int &channels() const { return chan; }


private:
    QString *error_msg;

    long elasped_seconds;
    unsigned long written_bytes;
    int brate, freq, prec, chan;
};


class OutputListeners
{
public:
    OutputListeners();
    virtual ~OutputListeners();

    void addListener(QObject *);
    void removeListener(QObject *);

    void addVisual(MythTV::Visual *);
    void removeVisual(MythTV::Visual *);
    
    QMutex *mutex() { return &mtx; }

    void setBufferSize(unsigned int sz) { bufsize = sz; }
    unsigned int bufferSize() const { return bufsize; }

protected:
    void dispatch(OutputEvent &e);
    void error(const QString &e);
    void dispatchVisual(uchar *b, unsigned long b_len, 
                       unsigned long written, int chan, int prec);
    void prepareVisuals();

private:
    QMutex mtx;
    QPtrList<QObject> listeners;
    QPtrList<MythTV::Visual> visuals;
    
    unsigned int bufsize;
};


#endif // __output_h
