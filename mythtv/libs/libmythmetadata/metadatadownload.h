#ifndef METADATADOWNLOAD_H
#define METADATADOWNLOAD_H

#include <QThread>
#include <QString>
#include <QStringList>

#include "metadatacommon.h"

typedef QList<MetadataLookup*> MetadataLookupList;

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

class META_PUBLIC MetadataDownload : public QThread
{
  public:

    MetadataDownload(QObject *parent);
    ~MetadataDownload();

    void addLookup(MetadataLookup *lookup);
    void prependLookup(MetadataLookup *lookup);
    void cancel();

  protected:

    void run();

  private:
    // Video handling
    MetadataLookupList  handleMovie(MetadataLookup* lookup);
    MetadataLookupList  handleTelevision(MetadataLookup* lookup);
    MetadataLookupList  handleVideoUndetermined(MetadataLookup* lookup);

    MetadataLookupList  handleGame(MetadataLookup* lookup);

    bool                findBestMatch(MetadataLookupList list,
                                      QString originaltitle);
    MetadataLookupList  runGrabber(QString cmd, QStringList args,
                                   MetadataLookup* lookup,
                                   bool passseas = true);
    MetadataLookup*     moreWork();

    QObject            *m_parent;
    MetadataLookupList  m_lookupList;
    QMutex              m_mutex;

};

#endif /* METADATADOWNLOAD_H */
