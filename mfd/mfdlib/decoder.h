#ifndef DECODER_H_
#define DECODER_H_
/*
	decoder.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for decoder classes
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include "../config.h"

#include <qstring.h>
#include <qevent.h>
#include <qthread.h>
#include <qptrlist.h>


#include "decoder_event.h"
#include "metadata.h"

#include "mfd_plugin.h"

class DecoderFactory;
class AudioOutput;
class OutputEvent;

class Decoder : public QThread 
{

  public:

    virtual ~Decoder();

    virtual bool initialize() = 0;
    void setParent(MFDServicePlugin *p){parent = p;}
    virtual void seek(double) = 0;
    virtual void stop() = 0;

    DecoderFactory *factory() const { return fctry; }

    //void addListener(QObject *);
    //void removeListener(QObject *);

    QIODevice *input() { return in; }
    AudioOutput *output() { return out; }
    void setInput(QIODevice *);
    void setOutput(AudioOutput *);

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

    virtual AudioMetadata *getMetadata() = 0;
    
    void metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre);
    
  protected:
    Decoder(DecoderFactory *, QIODevice *, AudioOutput *);

    void dispatch(const DecoderEvent &);
    void dispatch(const OutputEvent &);
    void error(const QString &);

    QString filename;

    QString filename_format;
    int ignore_id3;
    void log(const QString &log_message, int verbosity);
    void warning(const QString &warn_message);
    void message(const QString &internal_message);

    MFDServicePlugin *parent;

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

class WavDecoderFactory : public DecoderFactory
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

#ifdef WMA_AUDIO_SUPPORT
class avfDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, AudioOutput *, bool);
};
#endif

#ifdef AAC_AUDIO_SUPPORT
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
