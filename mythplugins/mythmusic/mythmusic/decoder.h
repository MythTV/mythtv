// -*- Mode: c++ -*-

#ifndef DECODER_H_
#define DECODER_H_

#include <QWaitCondition>
#include <QStringList>
#include <QEvent>
#include <QMutex>
#include <QCoreApplication>

#include "config.h"

#include <mythobservable.h>
#include <mthread.h>

class MusicMetadata;
class MetaIO;
class Decoder;
class DecoderFactory;

class QObject;
class QIODevice;
class OutputEvent;

class Buffer;
class Recycler;
class AudioOutput;

class DecoderEvent : public MythEvent
{
  public:
    DecoderEvent(Type t) : MythEvent(t), error_msg(NULL) { ; }

    DecoderEvent(QString *e) : MythEvent(Error), error_msg(e) { ; }

    ~DecoderEvent()
    {
        if (error_msg)
            delete error_msg;
    }

    const QString *errorMessage() const { return error_msg; }

    virtual MythEvent *clone(void) const { return new DecoderEvent(*this); }

    static Type Decoding;
    static Type Stopped;
    static Type Finished;
    static Type Error;

  private:
    DecoderEvent(const DecoderEvent &o) : MythEvent(o), error_msg(NULL)
    {
        if (o.error_msg)
        {
            error_msg = new QString(*o.error_msg);
            error_msg->detach();
        }
    }
    DecoderEvent &operator=(const DecoderEvent&);

  private:
    QString *error_msg;
};

class Decoder : public MThread, public MythObservable
{
  public:
    virtual ~Decoder();

    virtual bool initialize() = 0;
    virtual void seek(double) = 0;
    virtual void stop() = 0;

    DecoderFactory *factory() const { return m_fctry; }

    AudioOutput *output() { return m_out; }
    void setOutput(AudioOutput *);
    void setURL(const QString &url) { m_url = url; }

    virtual void lock(void) { return m_mtx.lock(); }
    virtual void unlock(void) { return m_mtx.unlock(); }
    virtual bool tryLock(void) { return m_mtx.tryLock(); }

    QWaitCondition *cond() { return &m_cnd; }

    QString getURL(void) const { return m_url; }

    // static methods
    static QStringList all();
    static bool supports(const QString &);
    static void registerFactory(DecoderFactory *);
    static Decoder *create(const QString &, AudioOutput *, bool = false);

  protected:
    Decoder(DecoderFactory *, AudioOutput *);
    QMutex* getMutex(void) { return &m_mtx; }
    void error(const QString &);

    QString m_url;

  private:
    DecoderFactory *m_fctry;

    AudioOutput *m_out;

    QMutex m_mtx;
    QWaitCondition m_cnd;

};

class DecoderFactory
{
public:
    virtual bool supports(const QString &source) const = 0;
    virtual const QString &extension() const = 0; // file extension, ie. ".mp3" or ".ogg"
    virtual const QString &description() const = 0; // file type, ie. "MPEG Audio Files"
    virtual Decoder *create(const QString &, AudioOutput *, bool) = 0;
    virtual ~DecoderFactory() {}
};

class CdDecoderFactory : public DecoderFactory
{
    Q_DECLARE_TR_FUNCTIONS(CdDecoderFactory)

public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, AudioOutput *, bool);
};

class avfDecoderFactory : public DecoderFactory
{
    Q_DECLARE_TR_FUNCTIONS(avfDecoderFactory)

public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, AudioOutput *, bool);
};

#endif
