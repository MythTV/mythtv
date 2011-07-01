#ifndef METADATAFACTORY_H_
#define METADATAFACTORY_H_

// Needed to perform a lookup
#include "metadatacommon.h"
#include "metadataimagedownload.h"
#include "metadatadownload.h"

// Symbol visibility
#include "mythmetaexp.h"

class VideoMetadata;

class META_PUBLIC MetadataFactoryMultiResult : public QEvent
{
  public:
    MetadataFactoryMultiResult(MetadataLookupList res) : QEvent(kEventType),
                                            results(res) {}
    ~MetadataFactoryMultiResult() {}

    MetadataLookupList results;

    static Type kEventType;
};

class META_PUBLIC MetadataFactorySingleResult : public QEvent
{
  public:
    MetadataFactorySingleResult(MetadataLookup *res) : QEvent(kEventType),
                                            result(res) {}
    ~MetadataFactorySingleResult() {}

    MetadataLookup *result;

    static Type kEventType;
};

class META_PUBLIC MetadataFactoryNoResult : public QEvent
{
  public:
    MetadataFactoryNoResult(MetadataLookup *res) : QEvent(kEventType),
                                            result(res) {}
    ~MetadataFactoryNoResult() {}

    MetadataLookup *result;

    static Type kEventType;
};

class META_PUBLIC MetadataFactory : public QObject
{

  public:

    MetadataFactory(QObject *parent);
    ~MetadataFactory();

    void Lookup(ProgramInfo *pginfo, bool automatic = true,
           bool getimages = true);
    void Lookup(VideoMetadata *metadata, bool automatic = true,
           bool getimages = true);
    void Lookup(MetadataLookup *lookup);

    bool IsRunning() { return m_lookupthread->isRunning() ||
                              m_imagedownload->isRunning(); };

  private:

    void customEvent(QEvent *levent);

    void OnMultiResult(MetadataLookupList list);
    void OnSingleResult(MetadataLookup *lookup);
    void OnNoResult(MetadataLookup *lookup);
    void OnImageResult(MetadataLookup *lookup);

    QObject *m_parent;
    MetadataDownload *m_lookupthread;
    MetadataImageDownload *m_imagedownload;
};

#endif
