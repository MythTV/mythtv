/*
	decoder.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for decoder classes
	
	Very closely based on work by Brad Hughes
	Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>

*/

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <mythtv/output.h>

#include "decoder.h"
#include "constants.h"
#include "buffer.h"
#include "visual.h"

Decoder::Decoder(DecoderFactory *d, QIODevice *i, AudioOutput *o)
       : fctry(d), in(i), out(o), blksize(0)
{
    parent = NULL;
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
        QApplication::postEvent(object, new DecoderEvent(e));
        object = listeners.next();
    }
}

void Decoder::dispatch(const OutputEvent &e)
{
    QObject *object = listeners.first();
    while (object) 
    {
        QApplication::postEvent(object, new OutputEvent(e));
        object = listeners.next();
    }
}

void Decoder::error(const QString &e) 
{
    QObject *object = listeners.first();
    while (object) 
    {
        QString *str = new QString(e.utf8());
        QApplication::postEvent(object, new DecoderEvent(str));
        object = listeners.next();
    }
}

/*
void Decoder::addListener(QObject *object)
{
    if (listeners.find(object) == -1)
        listeners.append(object);
}
*/

/*
void Decoder::removeListener(QObject *object)
{
    listeners.remove(object);
}
*/

void Decoder::metadataSanityCheck(QString *artist, QString *album, QString *title, QString *genre)
{
    if(artist->length() < 1)
    {
        artist->append("Unknown Artist");
    }
    
    if(album->length() < 1)
    {
        album->append("Unknown Album");
    }
    
    if(title->length() < 1)
    {
        title->append("Unknown Title");
    }
    
    if(genre->length() < 1)
    {
        genre->append("Unknown Genre");
    }
    
}


void Decoder::log(const QString &log_message, int verbosity)
{
    if(parent)
    {
        parent->log(log_message, verbosity);
    }
}

void Decoder::warning(const QString &warn_message)
{
    if(parent)
    {
        parent->warning(warn_message);
    }
}

void Decoder::message(const QString &internal_message)
{
    if(parent)
    {
        parent->sendInternalMessage(internal_message);
    }
}


// static methods

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
        Decoder::registerFactory(new WavDecoderFactory);
#ifdef WMA_AUDIO_SUPPORT
        Decoder::registerFactory(new avfDecoderFactory);
#endif
#ifdef AAC_AUDIO_SUPPORT
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

/*

void Decoder::getMetadataFromFilename(const QString filename,
                                      const QString regext, QString &artist, 
                                      QString &album, QString &title, 
                                      QString &genre, int &tracknum)
{
    // Ignore_ID3 header
    int part_num = 0;
    QStringList fmt_list = QStringList::split("/", filename_format);
    QStringList::iterator fmt_it = fmt_list.begin();

    // go through loop once to get minimum part number
    for(; fmt_it != fmt_list.end(); fmt_it++, part_num--);

    // reset to go through loop for real
    fmt_it = fmt_list.begin();
    for(; fmt_it != fmt_list.end(); fmt_it++, part_num++)
    {
        QString part_str = filename.section( "/", part_num, part_num);
        part_str.replace(QRegExp(QString("_")), QString(" "));
        part_str.replace(QRegExp(regext, FALSE), QString(""));

        if ( *fmt_it == "GENRE" )
            genre = part_str;
        else if ( *fmt_it == "ARTIST" )
            artist = part_str;
        else if ( *fmt_it == "ALBUM" ) 
            album = part_str;
        else if ( *fmt_it == "TITLE" )
            title = part_str;
        else if ( *fmt_it == "TRACK_TITLE" ) 
        {
            part_str.replace(QRegExp(QString("-")), QString(" "));
            QString s_tmp = part_str;
            s_tmp.replace(QRegExp(QString(" .*"), FALSE), QString(""));
            tracknum = s_tmp.toInt();
            title = part_str;
            title.replace(QRegExp(QString("^[0-9][0-9] "), FALSE),
                          QString(""));
            title = title.simplifyWhiteSpace();
        }
    }
}

*/
