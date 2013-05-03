// -*- Mode: c++ -*-

#ifndef DECODER_H_
#define DECODER_H_

#include <QWaitCondition>
#include <QStringList>
#include <QEvent>
#include <QMutex>

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

    DecoderFactory *factory() const { return fctry; }

    QIODevice *input(void);
    AudioOutput *output() { return out; }
    void setOutput(AudioOutput *);
    void setFilename(const QString &newName) { filename = newName; }

    virtual void lock(void) { return mtx.lock(); }
    virtual void unlock(void) { return mtx.unlock(); }
    virtual bool tryLock(void) { return mtx.tryLock(); }
    //virtual bool locked(void) { return mtx.locked(); }

    QWaitCondition *cond() { return &cnd; }

    QString getFilename(void) const { return filename; }

    // static methods
    static QStringList all();
    static bool supports(const QString &);
    static void registerFactory(DecoderFactory *);
    static Decoder *create(const QString &, AudioOutput *, bool = false);

  protected:
    Decoder(DecoderFactory *, AudioOutput *);
    QMutex* getMutex(void) { return &mtx; }
    void error(const QString &);

    QString filename;

  private:
    DecoderFactory *fctry;

    AudioOutput *out;

    QMutex mtx;
    QWaitCondition cnd;

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
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, AudioOutput *, bool);
};

class avfDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, AudioOutput *, bool);
};

#endif
