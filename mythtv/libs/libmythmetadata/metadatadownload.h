#ifndef METADATADOWNLOAD_H
#define METADATADOWNLOAD_H

#include <QStringList>
#include <QMutex>
#include <QEvent>

#include "metadatacommon.h"
#include "mthread.h"

class META_PUBLIC MetadataLookupEvent : public QEvent
{
  public:
    MetadataLookupEvent(MetadataLookupList lul) : QEvent(kEventType),
                                            lookupList(lul) {}
    ~MetadataLookupEvent() {}

    MetadataLookupList lookupList;

    static Type kEventType;
};

class META_PUBLIC MetadataLookupFailure : public QEvent
{
  public:
    MetadataLookupFailure(MetadataLookupList lul) : QEvent(kEventType),
                                            lookupList(lul) {}
    ~MetadataLookupFailure() {}

    MetadataLookupList lookupList;

    static Type kEventType;
};

class META_PUBLIC MetadataDownload : public MThread
{
  public:

    MetadataDownload(QObject *parent);
    ~MetadataDownload();

    void addLookup(MetadataLookup *lookup);
    void prependLookup(MetadataLookup *lookup);
    void cancel();

    static QString GetMovieGrabber();
    static QString GetTelevisionGrabber();
    static QString GetGameGrabber();

    bool runGrabberTest(const QString &grabberpath);
    bool MovieGrabberWorks();
    bool TelevisionGrabberWorks();

  protected:

    void run();

    QString getMXMLPath(QString filename);
    QString getNFOPath(QString filename);

  private:
    // Video handling
    MetadataLookupList  handleMovie(MetadataLookup* lookup);
    MetadataLookupList  handleTelevision(MetadataLookup* lookup);
    MetadataLookupList  handleVideoUndetermined(MetadataLookup* lookup);
    MetadataLookupList  handleRecordingGeneric(MetadataLookup* lookup);

    MetadataLookupList  handleGame(MetadataLookup* lookup);

    MetadataLookup*     findBestMatch(MetadataLookupList list,
                                      const QString &originaltitle) const;
    MetadataLookupList  runGrabber(QString cmd, QStringList args,
                                   MetadataLookup* lookup,
                                   bool passseas = true);
    MetadataLookupList  readMXML(QString MXMLpath,
                                 MetadataLookup* lookup,
                                 bool passseas = true);
    MetadataLookupList  readNFO(QString NFOpath, MetadataLookup* lookup);

    QObject            *m_parent;
    MetadataLookupList  m_lookupList;
    QMutex              m_mutex;

};

#endif /* METADATADOWNLOAD_H */
