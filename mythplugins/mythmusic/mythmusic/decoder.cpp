// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

// qt
#include <QDir>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmyth/output.h>
#include <libmyth/visual.h>
#include <libmythmetadata/metaio.h>
#include <libmythmetadata/musicmetadata.h>

// mythmusic
#include "config.h"
#include "decoder.h"
#include "constants.h"
#include "musicplayer.h"

const QEvent::Type DecoderEvent::kDecoding =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderEvent::kStopped =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderEvent::kFinished =
    (QEvent::Type) QEvent::registerEventType();
const QEvent::Type DecoderEvent::kError =
    (QEvent::Type) QEvent::registerEventType();

Decoder::~Decoder()
{
    m_fctry = nullptr;
    m_out = nullptr;
}

/*
QString Decoder::getURL(void)
{
    return gPlayer->getDecoderHandler()->getUrl();
}
*/

void Decoder::setOutput(AudioOutput *o)
{
    lock();
    m_out = o;
    unlock();
}

void Decoder::error(const QString &e)
{
    auto *str = new QString(e.toUtf8());
    DecoderEvent ev(str);
    dispatch(ev);
}

// static methods
static QList<DecoderFactory*> *factories = nullptr;

static void checkFactories()
{
    if (!factories)
    {
        factories = new QList<DecoderFactory*>;

#ifdef HAVE_CDIO
        Decoder::registerFactory(new CdDecoderFactory);
#endif
        Decoder::registerFactory(new avfDecoderFactory);
    }
}

QStringList Decoder::all()
{
    checkFactories();

    return std::accumulate(factories->cbegin(), factories->cend(),
                           QStringList(),
                           [](QStringList& l, const auto & factory)
                               { return l += factory->description(); } );
}

bool Decoder::supports(const QString &source)
{
    checkFactories();

    return std::any_of(factories->cbegin(), factories->cend(),
                       [source](const auto & factory)
                           {return factory->supports(source); } );
}

void Decoder::registerFactory(DecoderFactory *fact)
{
    factories->push_back(fact);
}

Decoder *Decoder::create(const QString &source, AudioOutput *output, bool deletable)
{
    checkFactories();

    auto supported = [source](const auto & factory)
        { return factory->supports(source); };
    auto f = std::find_if(factories->cbegin(), factories->cend(), supported);
    return (f != factories->cend())
        ? (*f)->create(source, output, deletable)
        : nullptr;
}
