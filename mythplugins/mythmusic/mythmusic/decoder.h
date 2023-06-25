// -*- Mode: c++ -*-

#ifndef DECODER_H_
#define DECODER_H_

// Qt
#include <QCoreApplication>
#include <QEvent>
#include <QMutex>
#include <QStringList>
#include <QWaitCondition>

// MythTV
#include <libmythbase/mthread.h>
#include <libmythbase/mythobservable.h>

class MusicMetadata;
class MetaIO;
class Decoder;
class DecoderFactory;

class QObject;
class QIODevice;
class OutputEvent;

class Buffer;
class AudioOutput;

class DecoderEvent : public MythEvent
{
  public:
    explicit DecoderEvent(Type type) : MythEvent(type) { ; }
    explicit DecoderEvent(QString *e) : MythEvent(kError), m_errorMsg(e) { ; }

    ~DecoderEvent() override
    {
        delete m_errorMsg;
    }

    const QString *errorMessage() const { return m_errorMsg; }

    MythEvent *clone(void) const override // MythEvent
        { return new DecoderEvent(*this); }

    static const Type kDecoding;
    static const Type kStopped;
    static const Type kFinished;
    static const Type kError;

  private:
    DecoderEvent(const DecoderEvent &o) : MythEvent(o)
    {
        if (o.m_errorMsg)
        {
            m_errorMsg = new QString(*o.m_errorMsg);
        }
    }

  // No implicit copying.
  protected:
    DecoderEvent &operator=(const DecoderEvent &other) = default;
  public:
    DecoderEvent(DecoderEvent &&) = delete;
    DecoderEvent &operator=(DecoderEvent &&) = delete;

  private:
    QString *m_errorMsg {nullptr};
};

class Decoder : public MThread, public MythObservable
{
  public:
    ~Decoder() override;

    virtual bool initialize() = 0;
    virtual void seek(double) = 0;
    virtual void stop() = 0;

    DecoderFactory *factory() const { return m_fctry; }

    AudioOutput *output() { return m_out; }
    void setOutput(AudioOutput *o);
    void setURL(const QString &url) { m_url = url; }

    virtual void lock(void) { m_mtx.lock(); }
    virtual void unlock(void) { m_mtx.unlock(); }
    virtual bool tryLock(void) { return m_mtx.tryLock(); }

    QWaitCondition *cond() { return &m_cnd; }

    QString getURL(void) const { return m_url; }

    // static methods
    static QStringList all();
    static bool supports(const QString &source);
    static void registerFactory(DecoderFactory *fact);
    static Decoder *create(const QString &source, AudioOutput *output, bool deletable = false);

  protected:
    Decoder(DecoderFactory *d, AudioOutput *o)
        : MThread("MythMusicDecoder"), m_fctry(d), m_out(o) {}
    QMutex* getMutex(void) { return &m_mtx; }
    void error(const QString &e);

    QString m_url;

  private:
    DecoderFactory *m_fctry {nullptr};

    AudioOutput *m_out {nullptr};

    QMutex m_mtx;
    QWaitCondition m_cnd;

};

class DecoderFactory
{
public:
    virtual bool supports(const QString &source) const = 0;
    virtual const QString &extension() const = 0; // file extension, ie. ".mp3" or ".ogg"
    virtual const QString &description() const = 0; // file type, ie. "MPEG Audio Files"
    virtual Decoder *create(const QString &source, AudioOutput *output, bool deletable) = 0;
    virtual ~DecoderFactory() = default;
};

class CdDecoderFactory : public DecoderFactory
{
    Q_DECLARE_TR_FUNCTIONS(CdDecoderFactory);

public:
    bool supports(const QString &source) const override; // DecoderFactory
    const QString &extension() const override; // DecoderFactory
    const QString &description() const override; // DecoderFactory
    Decoder *create(const QString &file, AudioOutput *output, bool deletable) override; // DecoderFactory
};

class avfDecoderFactory : public DecoderFactory
{
    Q_DECLARE_TR_FUNCTIONS(avfDecoderFactory);

public:
    bool supports(const QString &source) const override; // DecoderFactory
    const QString &extension() const override; // DecoderFactory
    const QString &description() const override; // DecoderFactory
    Decoder *create(const QString &file, AudioOutput *output, bool deletable) override; // DecoderFactory
};

#endif
