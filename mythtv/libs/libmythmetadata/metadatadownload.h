#ifndef METADATADOWNLOAD_H
#define METADATADOWNLOAD_H

#include <QStringList>
#include <QMutex>
#include <QEvent>

#include "libmythbase/mthread.h"
#include "libmythmetadata/metadatacommon.h"

class META_PUBLIC MetadataLookupEvent : public QEvent
{
  public:
    explicit MetadataLookupEvent(const MetadataLookupList& lul) : QEvent(kEventType),
                                            m_lookupList(lul) {}
    ~MetadataLookupEvent() override = default;

    MetadataLookupList m_lookupList;

    static const Type kEventType;
};

class META_PUBLIC MetadataLookupFailure : public QEvent
{
  public:
    explicit MetadataLookupFailure(const MetadataLookupList& lul) : QEvent(kEventType),
                                            m_lookupList(lul) {}
    ~MetadataLookupFailure() override = default;

    MetadataLookupList m_lookupList;

    static const Type kEventType;
};

class META_PUBLIC MetadataDownload : public MThread
{
  public:

    explicit MetadataDownload(QObject *parent)
        : MThread("MetadataDownload"), m_parent(parent) {}
    ~MetadataDownload() override;

    void addLookup(MetadataLookup *lookup);
    void prependLookup(MetadataLookup *lookup);
    void cancel();

    static QString GetMovieGrabber();
    static QString GetTelevisionGrabber();
    static QString GetGameGrabber();

    static bool runGrabberTest(const QString &grabberpath);
    static bool MovieGrabberWorks();
    static bool TelevisionGrabberWorks();

  protected:

    void run() override; // MThread

    static QString getMXMLPath(const QString& filename);
    static QString getNFOPath(const QString& filename);

  private:
    // Video handling
    static MetadataLookupList  handleMovie(MetadataLookup* lookup);
    static MetadataLookupList  handleTelevision(MetadataLookup* lookup);
    static MetadataLookupList  handleVideoUndetermined(MetadataLookup* lookup);
    static MetadataLookupList  handleRecordingGeneric(MetadataLookup* lookup);

    static MetadataLookupList  handleGame(MetadataLookup* lookup);

    static unsigned int        findExactMatchCount(MetadataLookupList list,
                                                   const QString &originaltitle,
                                                   bool withArt) ;
    static MetadataLookup*     findBestMatch(MetadataLookupList list,
                                             const QString &originaltitle) ;
    static MetadataLookupList  runGrabber(const QString& cmd,
                                          const QStringList& args,
                                          MetadataLookup* lookup,
                                          bool passseas = true);
    static MetadataLookupList  readMXML(const QString& MXMLpath,
                                        MetadataLookup* lookup,
                                        bool passseas = true);
    static MetadataLookupList  readNFO(const QString& NFOpath,
                                       MetadataLookup* lookup);

    QObject            *m_parent {nullptr};
    MetadataLookupList  m_lookupList;
    QMutex              m_mutex;
};

#endif /* METADATADOWNLOAD_H */
