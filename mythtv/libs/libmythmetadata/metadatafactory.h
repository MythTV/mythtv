#ifndef METADATAFACTORY_H_
#define METADATAFACTORY_H_

// Needed to perform a lookup
#include "metadatacommon.h"
#include "metadataimagedownload.h"
#include "metadatadownload.h"

// Needed to perform scans
#include "videoscan.h"

// Symbol visibility
#include "mythmetaexp.h"

class VideoMetadata;
class RecordingRule;

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
                                            result(res)
    {
        if (result)
        {
            result->IncrRef();
        }
    }
    ~MetadataFactorySingleResult()
    {
        if (result)
        {
            result->DecrRef();
            result = NULL;
        }
    }

    MetadataLookup *result;

    static Type kEventType;
};

class META_PUBLIC MetadataFactoryNoResult : public QEvent
{
  public:
    MetadataFactoryNoResult(MetadataLookup *res) : QEvent(kEventType),
                                            result(res)
    {
        if (result)
        {
            result->IncrRef();
        }
    }
    ~MetadataFactoryNoResult()
    {
        if (result)
        {
            result->DecrRef();
            result = NULL;
        }
    }

    MetadataLookup *result;

    static Type kEventType;
};

class META_PUBLIC MetadataFactoryVideoChanges : public QEvent
{
  public:
    MetadataFactoryVideoChanges(QList<int> adds, QList<int> movs,
                                QList<int>dels) : QEvent(kEventType),
                                additions(adds), moved(movs),
                                deleted(dels) {}
    ~MetadataFactoryVideoChanges() {}

    QList<int> additions; // newly added intids
    QList<int> moved; // intids moved to new filename
    QList<int> deleted; // orphaned/deleted intids

    static Type kEventType;
};

class META_PUBLIC MetadataFactory : public QObject
{

  public:

    MetadataFactory(QObject *parent);
    ~MetadataFactory();

    void Lookup(ProgramInfo *pginfo, bool automatic = true,
           bool getimages = true, bool allowgeneric = false);
    void Lookup(VideoMetadata *metadata, bool automatic = true,
           bool getimages = true, bool allowgeneric = false);
    void Lookup(RecordingRule *recrule, bool automatic = true,
           bool getimages = true, bool allowgeneric = false);
    void Lookup(MetadataLookup *lookup);

    MetadataLookupList SynchronousLookup(QString title,
                                         QString subtitle,
                                         QString inetref,
                                         int season,
                                         int episode,
                                         QString grabber,
                                         bool allowgeneric = false);
    MetadataLookupList SynchronousLookup(MetadataLookup *lookup);

    void VideoScan();
    void VideoScan(QStringList hosts);

    bool IsRunning() { return m_lookupthread->isRunning() ||
                              m_imagedownload->isRunning() ||
                              m_videoscanner->isRunning(); };

    bool VideoGrabbersFunctional();

  private:

    void customEvent(QEvent *levent);

    void OnMultiResult(MetadataLookupList list);
    void OnSingleResult(MetadataLookup *lookup);
    void OnNoResult(MetadataLookup *lookup);
    void OnImageResult(MetadataLookup *lookup);

    void OnVideoResult(MetadataLookup *lookup);

    MetadataDownload *m_lookupthread;
    MetadataImageDownload *m_imagedownload;

    VideoScannerThread *m_videoscanner;
    VideoMetadataListManager *m_mlm;
    bool m_scanning;

    // Variables used in synchronous mode
    MetadataLookupList m_returnList;
    bool m_sync;
};

META_PUBLIC LookupType GuessLookupType(ProgramInfo *pginfo);
META_PUBLIC LookupType GuessLookupType(MetadataLookup *lookup);
META_PUBLIC LookupType GuessLookupType(VideoMetadata *metadata);
META_PUBLIC LookupType GuessLookupType(RecordingRule *recrule);

#endif
