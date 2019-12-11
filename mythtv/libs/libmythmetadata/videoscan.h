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
#include "mythprogressdialog.h"

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
    class VideoScannerThread *m_scanThread {nullptr};
    bool                      m_cancel     {false};
};

class META_PUBLIC VideoScanChanges : public QEvent
{
  public:
    VideoScanChanges(QList<int> adds, QList<int> movs,
                     QList<int>dels) : QEvent(kEventType),
                     m_additions(adds), m_moved(movs),
                     m_deleted(dels) {}
    ~VideoScanChanges() = default;

    QList<int> m_additions; // newly added intids
    QList<int> m_moved; // intids moved to new filename
    QList<int> m_deleted; // orphaned/deleted intids

    static Type kEventType;
};

class META_PUBLIC VideoScannerThread : public MThread
{
    Q_DECLARE_TR_FUNCTIONS(VideoScannerThread);

  public:
    explicit VideoScannerThread(QObject *parent);
    ~VideoScannerThread();

    void run() override; // MThread
    void SetDirs(QStringList dirs);
    void SetHosts(const QStringList &hosts);
    void SetProgressDialog(MythUIProgressDialog *dialog) { m_dialog = dialog; };
    QStringList GetOfflineSGHosts(void) { return m_offlineSGHosts; };
    bool getDataChanged() { return m_dbDataChanged; };

    void ResetCounts() { m_addList.clear(); m_movList.clear(); m_delList.clear(); };

  private:

    struct CheckStruct
    {
        bool check;
        QString host;
    };

    using PurgeList = std::vector<std::pair<unsigned int, QString> >;
    using FileCheckList = std::map<QString, CheckStruct>;

    void removeOrphans(unsigned int id, const QString &filename);

    void verifyFiles(FileCheckList &files, PurgeList &remove);
    bool updateDB(const FileCheckList &add, const PurgeList &remove);
    bool buildFileList(const QString &directory,
                                        const QStringList &imageExtensions,
                                        FileCheckList &filelist);

    void SendProgressEvent(uint progress, uint total = 0,
            QString messsage = QString());

    QObject *m_parent    {nullptr};

    bool m_listUnknown   {false};
    bool m_removeAll     {false};
    bool m_keepAll       {false};
    bool m_hasGUI        {false};
    QStringList m_directories;
    QStringList m_liveSGHosts;
    QStringList m_offlineSGHosts;

    VideoMetadataListManager *m_dbMetadata {nullptr};
    MythUIProgressDialog     *m_dialog     {nullptr};

    QList<int> m_addList; // newly added intids
    QList<int> m_movList; // intids moved to new filename
    QList<int> m_delList; // orphaned/deleted intids
    bool m_dbDataChanged {false};
};

#endif
