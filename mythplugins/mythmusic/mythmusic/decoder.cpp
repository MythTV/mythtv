// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "decoder.h"
#include "constants.h"
#include "metadata.h"
#include "metaio.h"
#include <mythtv/output.h>
#include <mythtv/visual.h>

#include <qapplication.h>
#include <qobject.h>
#include <qptrlist.h>
#include <qdir.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <mythtv/mythcontext.h>

DecoderEvent* DecoderEvent::clone() 
{ 
    DecoderEvent *result = new DecoderEvent(*this);

    if (error_msg)
        result->error_msg = new QString(*error_msg);

    return result;
}

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
    lock();
    in = i;
    unlock();
}

void Decoder::setOutput(AudioOutput *o)
{
    lock();
    out = o;
    unlock();
}

void Decoder::error(const QString &e) 
{
    QString *str = new QString(e.utf8());
    DecoderEvent ev(str);
    dispatch(ev);
}

/** \fn Decoder::readMetadata(void)
 *  \brief Read the metadata from \p filename directly.
 *
 *  Creates a \p MetaIO object using \p Decoder::doCreateTagger and uses
 *  the MetaIO object to read the metadata.
 *  \returns an instance of \p Metadata owned by the caller
 */
Metadata *Decoder::readMetadata(void)
{
    Metadata *mdata = NULL;
    MetaIO* p_tagger = doCreateTagger();

    if (p_tagger)
    {
        if (ignore_id3)
            mdata = p_tagger->readFromFilename(filename);
        else
            mdata = p_tagger->read(filename);

        delete p_tagger;
    }
    else if (!mdata)
    {
        VERBOSE(VB_IMPORTANT, "Decoder::readMetadata(): " +
                QString("Could not read '%1'").arg(filename));
    }

    return mdata;
}


/** \fn Decoder::getMetadata(void)
 *  \brief Get the metadata  for \p filename 
 *
 *  First tries to read the metadata from the database. If there
 *  is no database entry, it'll call \p Decoder::readMetadata.
 *
 *  \returns an instance of \p Metadata owned by the caller
 */
Metadata* Decoder::getMetadata(void)
{

    Metadata *mdata = new Metadata(filename);
    if (mdata->isInDatabase(musiclocation))
    {
        return mdata;
    }

    delete mdata;

    return readMetadata();
}


/** \fn commitMetadata(Metadata*)
 *  \brief Create a \p MetaIO object for the format.
 *
 *  This method should be overwritten by subclasses to return an
 *  instance of the appropriate MetaIO subtype. It is used by \p
 *  Decoder::getMetadata, \p Decoder::readMetadata and \p
 *  Decoder::commitMetadata.
 *       
 *  The default implementation returns a NULL pointer, which
 *  essentially means, that if the decoder does not overrider this
 *  method or all of the users (see previous paragraph), files
 *  that the decoder supports cannot be indexed using metadata in
 *  the file.
 *
 *  e.g. the mp3 decoder (\p MadDecoder) implements this, whereas
 *  the audio CD decoder (\p CdDecoder) does not.
 *
 *  \returns an instance of \p MetaIO owned by the caller
 */
MetaIO *Decoder::doCreateTagger(void)
{
    return NULL;
}

/** \fn commitMetadata(Metadata*)
 *  \brief Write the given metadata to the \p filename.
 *
 *  Creates a \p MetaIO object using \p Decoder::doCreateTagger and
 *  asks the MetaIO object to write the contents of mdata to \p
 *  filename.
 *
 *  \params mdata the metadata to write to the disk
 */
void Decoder::commitMetadata(Metadata *mdata)
{
    MetaIO* p_tagger = doCreateTagger();
    if (p_tagger)
    {
        p_tagger->write(mdata);
        delete p_tagger;
    }
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


