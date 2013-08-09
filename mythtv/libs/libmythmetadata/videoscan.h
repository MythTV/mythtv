#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <set>
#include <map>
#include <vector>

#include <QObject> // for moc
#include <QStringList>
#include <QEvent>
#include <QCoreApplication>

#include "mythmetaexp.h"
#include "mthread.h"

class QStringList;

class MythUIProgressDialog;

class VideoMetadataListManager;

class META_PUBLIC VideoScanner : public QObject
{
    Q_OBJECT

  public:
    VideoScanner();
    ~VideoScanner();

    void doScan(const QStringList &dirs);
    void doScanAll(void);

  signals:
    void finished(bool);

  public slots:
    void finishedScan();

  private:
    class VideoScannerThread *m_scanThread;
    bool                      m_cancel;
};

class META_PUBLIC VideoScanChanges : public QEvent
{
  public:
    VideoScanChanges(QList<int> adds, QList<int> movs,
                     QList<int>dels) : QEvent(kEventType),
                     additions(adds), moved(movs),
                     deleted(dels) {}
    ~VideoScanChanges() {}

    QList<int> additions; // newly added intids
    QList<int> moved; // intids moved to new filename
    QList<int> deleted; // orphaned/deleted intids

    static Type kEventType;
};

class META_PUBLIC VideoScannerThread : public MThread
{
    Q_DECLARE_TR_FUNCTIONS(VideoScannerThread)

  public:
    VideoScannerThread(QObject *parent);
    ~VideoScannerThread();

    void run();
    void SetDirs(QStringList dirs);
    void SetHosts(const QStringList &hosts);
    void SetProgressDialog(MythUIProgressDialog *dialog) { m_dialog = dialog; };
    QStringList GetOfflineSGHosts(void) { return m_offlineSGHosts; };
    bool getDataChanged() { return m_DBDataChanged; };

    void ResetCounts() { m_addList.clear(); m_movList.clear(); m_delList.clear(); };

  private:

    struct CheckStruct
    {
        bool check;
        QString host;
    };

    typedef std::vector<std::pair<unsigned int, QString> > PurgeList;
    typedef std::map<QString, CheckStruct> FileCheckList;

    void removeOrphans(unsigned int id, const QString &filename);

    void verifyFiles(FileCheckList &files, PurgeList &remove);
    bool updateDB(const FileCheckList &add, const PurgeList &remove);
    bool buildFileList(const QString &directory,
                                        const QStringList &imageExtensions,
                                        FileCheckList &filelist);

    void SendProgressEvent(uint progress, uint total = 0,
            QString messsage = QString());

    QObject *m_parent;

    bool m_ListUnknown;
    bool m_RemoveAll;
    bool m_KeepAll;
    bool m_HasGUI;
    QStringList m_directories;
    QStringList m_liveSGHosts;
    QStringList m_offlineSGHosts;

    VideoMetadataListManager *m_dbmetadata;
    MythUIProgressDialog *m_dialog;

    QList<int> m_addList; // newly added intids
    QList<int> m_movList; // intids moved to new filename
    QList<int> m_delList; // orphaned/deleted intids
    bool m_DBDataChanged;
};

#endif
