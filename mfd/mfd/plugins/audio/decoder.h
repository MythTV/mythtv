#ifndef DECODER_H_
#define DECODER_H_
/*
	decoder.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for decoder classes
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/


#include <qstring.h>
#include <qevent.h>
#include <qthread.h>
#include <qptrlist.h>

#include "output.h"
#include "decoder_event.h"

//  class Metadata;
class DecoderFactory;

//  class QSqlDatabase;
//  class QObject;
//  class QIODevice;
//  class QThread;

//  class OutputEvent;

//  class Buffer;
//  class Recycler;
//  class Output;

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
    Output *output() { return out; }
    void setInput(QIODevice *);
    void setOutput(Output *);

    QMutex *mutex() { return &mtx; }
    QWaitCondition *cond() { return &cnd; }

    void setBlockSize(unsigned int sz) { blksize = sz; }
    unsigned int blockSize() const { return blksize; }

    // static methods
    static QStringList all();
    static bool supports(const QString &);
    static void registerFactory(DecoderFactory *);
    static Decoder *create(const QString &, QIODevice *, Output *, 
                           bool = FALSE);

    //  virtual Metadata *getMetadata(QSqlDatabase *db) = 0;
    //  virtual void commitMetadata(Metadata *mdata) = 0;

  protected:
    Decoder(DecoderFactory *, QIODevice *, Output *);

    void dispatch(const DecoderEvent &);
    void dispatch(const OutputEvent &);
    void error(const QString &);

    QString filename;

    /*
    void getMetadataFromFilename(const QString filename, const QString regext,
                                 QString &artist, QString &album,
                                 QString &title, QString &genre, int &tracknum);
    */

    QString filename_format;
    int ignore_id3;

  private:
    DecoderFactory *fctry;

    QPtrList<QObject> listeners;
    QIODevice *in;
    Output *out;

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
    virtual Decoder *create(const QString &, QIODevice *, Output *, bool) = 0;
};

class VorbisDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, Output *, bool);
};

class MadDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, Output *, bool);
};


class CdDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, Output *, bool);
};

class FlacDecoderFactory : public DecoderFactory
{
public:
    bool supports(const QString &) const;
    const QString &extension() const;
    const QString &description() const;
    Decoder *create(const QString &, QIODevice *, Output *, bool);
};


#endif
