// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "decoder.h"
#include "constants.h"
#include <mythtv/output.h>
#include <mythtv/visual.h>

#include <qobject.h>
#include <qptrlist.h>
#include <qdir.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <mythtv/mythcontext.h>

Decoder::Decoder(DecoderFactory *d, QIODevice *i, AudioOutput *o)
       : fctry(d), in(i), out(o), blksize(0)
{
}

Decoder::~Decoder()
{
    fctry = 0;
    in = 0;
    out = 0;
    blksize = 0;
}

void Decoder::setInput(QIODevice *i)
{
    mutex()->lock();
    in = i;
    mutex()->unlock();
}

void Decoder::setOutput(AudioOutput *o)
{
    mutex()->lock();
    out = o;
    mutex()->unlock();
}

void Decoder::dispatch(const DecoderEvent &e)
{
    QObject *object = listeners.first();
    while (object) 
    {
        QThread::postEvent(object, new DecoderEvent(e));
        object = listeners.next();
    }
}

void Decoder::dispatch(const OutputEvent &e)
{
    QObject *object = listeners.first();
    while (object) 
    {
        QThread::postEvent(object, new OutputEvent(e));
        object = listeners.next();
    }
}

void Decoder::error(const QString &e) 
{
    QObject *object = listeners.first();
    while (object) 
    {
        QString *str = new QString(e.utf8());
        QThread::postEvent(object, new DecoderEvent(str));
        object = listeners.next();
    }
}

void Decoder::addListener(QObject *object)
{
    if (listeners.find(object) == -1)
        listeners.append(object);
}


void Decoder::removeListener(QObject *object)
{
    listeners.remove(object);
}


// static methods

int Decoder::ignore_id3 = 0;
QString Decoder::musiclocation = "";

void Decoder::SetLocationFormatUseTags(void)
{
    QString startdir = gContext->GetSetting("MusicLocation");
    startdir = QDir::cleanDirPath(startdir);
    if (!startdir.endsWith("/"))
        startdir += "/";

    musiclocation = startdir;

    ignore_id3 = gContext->GetNumSetting("Ignore_ID3", 0);
}

static QPtrList<DecoderFactory> *factories = 0;

static void checkFactories() 
{
    if (!factories) 
    {
        factories = new QPtrList<DecoderFactory>;

        Decoder::registerFactory(new VorbisDecoderFactory);
        Decoder::registerFactory(new MadDecoderFactory);
        Decoder::registerFactory(new FlacDecoderFactory);
        Decoder::registerFactory(new CdDecoderFactory);
        Decoder::registerFactory(new avfDecoderFactory);
#ifdef AAC_SUPPORT
        Decoder::registerFactory(new aacDecoderFactory);
#endif
    }
}

QStringList Decoder::all()
{
    checkFactories();

    QStringList l;
    DecoderFactory *fact = factories->first();
    while (fact) 
    {
        l << fact->description();
        fact = factories->next();
    }

    return l;
}

bool Decoder::supports(const QString &source)
{
    checkFactories();

    DecoderFactory *fact = factories->first();
    while (fact) 
    {
        if (fact->supports(source))
            return TRUE;

        fact = factories->next();
    }

    return FALSE;
}

void Decoder::registerFactory(DecoderFactory *fact)
{
    factories->append(fact);
}

Decoder *Decoder::create(const QString &source, QIODevice *input,
                         AudioOutput *output, bool deletable)
{
    checkFactories();

    Decoder *decoder = 0;

    DecoderFactory *fact = factories->first();
    while (fact) 
    {
        if (fact->supports(source)) 
        {
            decoder = fact->create(source, input, output, deletable);
            break;
        }

        fact = factories->next();
    }

    return decoder;
}


