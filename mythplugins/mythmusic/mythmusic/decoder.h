#ifndef DECODER_H_
#define DECODER_H_

#include "config.h"
#include <qstring.h>
#include <qevent.h>
#include <qthread.h>
#include <qptrlist.h>

class Metadata;
class Decoder;
class DecoderFactory;

class QObject;
class QIODevice;
class QThread;
class OutputEvent;

class Buffer;
class Recycler;
class AudioOutput;

class DecoderEvent : public QCustomEvent
{
public:
    enum Type { Decoding = (QEvent::User + 100), Stopped, Finished, Error };

    DecoderEvent(Type t)
        : QCustomEvent(t), error_msg(0)
    { ; }

    DecoderEvent(QString *e)
        : QCustomEvent(Error), error_msg(e)
    { ; }

    ~DecoderEvent()
    {
        if (error_msg)
            delete error_msg;
    }

    const QString *errorMessage() const { return error_msg; }


private:
    QString *error_msg;
};

class Decoder : public QThread 
{
  public:
    virtual ~Decoder();

    virtual bool initialize() = 0;
    virtual void seek(double) = 0;
    virtual void stop() = 0;

    DecoderFactory *factory() const { return fctry; }

    void addListener(QObject *);
    void removeListener(QObject *);

    QIODevice *input() { return in; }
    AudioOutput *output() { return out; }
    void setInput(QIODevice *);
    void setOutput(AudioOutput *);
    void setFilename(const QString &newName) { filename = newName; }

    QMutex *mutex() { return &mtx; }
    QWaitCondition *cond() { return &cnd; }

    void setBlockSize(unsigned int sz) { blksize = sz; }
    unsigned int blockSize() const { return blksize; }

    // static methods
    static QStringList all();
    static bool supports(const QString &);
    static void registerFactory(DecoderFactory *);
    static Decoder *create(const QString &, QIODevice *, AudioOutput *, 
                           bool = FALSE);

    virtual Metadata *getMetadata() = 0;
    virtual void commitMetadata(Metadata *mdata) = 0;

    static void SetLocationFormatUseTags(void);

    QString getFilename(void) { return filename; }

  protected:
    Decoder(DecoderFactory *, QIODevice *, AudioOutput *);

    void dispatch(const DecoderEvent &);
    void dispatch(const OutputEvent &);
    void error(const QString &);

    QString filename;

    static QString filename_format;
    static int ignore_id3;
    static QString musiclocation;

  private:
    DecoderFactory *fctry;

    QPtrList<QObject> listeners;
    QIODevice *in;
    AudioOutput *out;

    QMutex mtx;
    QWaitCondition cnd;

    int blksize;
};

class DecoderFactory
{
public:
    virtual bool supports(const QString &source) const = 0;
    virtual const QString &extension() const = 0; // file extension, ie. ".mp3" or ".ogg"
    virtual const QString &description() const = 0; // file type, ie. "MPEG Audio Files"
    virtual Decoder *create(const QString &, QIODevice *, AudioOutput *, bool) = 0;
};

class VorbisDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};

class MadDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};

class CdDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};

class FlacDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};

class avfDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};

#ifdef AAC_SUPPORT
class aacDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};
#endif

#endif
