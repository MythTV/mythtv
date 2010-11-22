// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#include "decoder.h"
#include "constants.h"
#include "metadata.h"
#include "metaio.h"

#include <QDir>

#include <mythcontext.h>
#include <output.h>
#include <visual.h>

QEvent::Type DecoderEvent::Decoding =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderEvent::Stopped =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderEvent::Finished =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type DecoderEvent::Error =
    (QEvent::Type) QEvent::registerEventType();

Decoder::Decoder(DecoderFactory *d, QIODevice *i, AudioOutput *o)
       : fctry(d), in(i), out(o)
{
}

Decoder::~Decoder()
{
    fctry = 0;
    in = 0;
    out = 0;
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
    QString *str = new QString(e.toUtf8());
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
        if (!ignore_id3)
            mdata = p_tagger->read(filename);

        if (ignore_id3 || !mdata)
            mdata = p_tagger->readFromFilename(filename);

        delete p_tagger;
    }

    if (!mdata)
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
    if (mdata->isInDatabase())
    {
        return mdata;
    }

    delete mdata;

    return readMetadata();
}


/**
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
 *  e.g. the avf decoder (\p AvfDecoder) implements this, whereas
 *  the audio CD decoder (\p CdDecoder) does not.
 *
 *  \returns an instance of \p MetaIO owned by the caller
 */
MetaIO *Decoder::doCreateTagger(void)
{
    return NULL;
}

/**
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

/**
 *  \brief Write the changable metadata, e.g. ratings, playcounts; to the
 *         \p filename if the tag format supports it.
 *
 *  Creates a \p MetaIO object using \p Decoder::doCreateTagger and
 *  asks the MetaIO object to write changes to a specific subset of metadata
 *  to \p filename.
 *
 *  \params mdata the metadata to write to the disk
 */
void Decoder::commitVolatileMetadata(const Metadata *mdata)
{
    if (!mdata || !GetMythDB()->GetNumSetting("AllowTagWriting", 0))
        return;

    MetaIO* p_tagger = doCreateTagger();
    if (p_tagger)
    {
        p_tagger->writeVolatileMetadata(mdata);
        delete p_tagger;
    }

    mdata->UpdateModTime();
}

// static methods

int Decoder::ignore_id3 = 0;
QString Decoder::musiclocation;

void Decoder::SetLocationFormatUseTags(void)
{
    QString startdir = gCoreContext->GetSetting("MusicLocation");
    startdir = QDir::cleanPath(startdir);
    if (!startdir.endsWith("/"))
        startdir += "/";

    musiclocation = startdir;

    ignore_id3 = gCoreContext->GetNumSetting("Ignore_ID3", 0);
}

static QList<DecoderFactory*> *factories = NULL;

static void checkFactories()
{
    if (!factories)
    {
        factories = new QList<DecoderFactory*>;

#ifndef USING_MINGW
        Decoder::registerFactory(new CdDecoderFactory);
#endif // USING_MINGW
        Decoder::registerFactory(new avfDecoderFactory);
    }
}

QStringList Decoder::all()
{
    checkFactories();

    QStringList l;

    QList<DecoderFactory*>::iterator it = factories->begin();
    for (; it != factories->end(); ++it)
        l += (*it)->description();

    return l;
}

bool Decoder::supports(const QString &source)
{
    checkFactories();

    QList<DecoderFactory*>::iterator it = factories->begin();
    for (; it != factories->end(); ++it)
    {
        if ((*it)->supports(source))
            return true;
    }

    return false;
}

void Decoder::registerFactory(DecoderFactory *fact)
{
    factories->push_back(fact);
}

Decoder *Decoder::create(const QString &source, QIODevice *input,
                         AudioOutput *output, bool deletable)
{
    checkFactories();

    QList<DecoderFactory*>::iterator it = factories->begin();
    for (; it != factories->end(); ++it)
    {
        if ((*it)->supports(source))
            return (*it)->create(source, input, output, deletable);
    }

    return NULL;
}


