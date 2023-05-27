#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <map>
#include <set>
#include <utility>
#include <vector>

// Qt headers
#include <QObject> // for moc
#include <QStringList>
#include <QEvent>
#include <QCoreApplication>

// MythTV headers
#include "libmythbase/mthread.h"
#include "libmythmetadata/mythmetaexp.h"
#include "libmythui/mythprogressdialog.h"

class VideoMetadataListManager;

class META_PUBLIC VideoScanner : public QObject
{
    Q_OBJECT

  public:
    VideoScanner();
    ~VideoScanner() override;

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
                     m_additions(std::move(adds)), m_moved(std::move(movs)),
                     m_deleted(std::move(dels)) {}
    ~VideoScanChanges() override = default;

    QList<int> m_additions; // newly added intids
    QList<int> m_moved; // intids moved to new filename
    QList<int> m_deleted; // orphaned/deleted intids

    static const Type kEventType;
};

class META_PUBLIC VideoScannerThread : public MThread
{
    Q_DECLARE_TR_FUNCTIONS(VideoScannerThread);

  public:
    explicit VideoScannerThread(QObject *parent);
    ~VideoScannerThread() override;

    void run() override; // MThread
    void SetDirs(QStringList dirs);
    void SetHosts(const QStringList &hosts);
    void SetProgressDialog(MythUIProgressDialog *dialog) { m_dialog = dialog; };
    QStringList GetOfflineSGHosts(void) { return m_offlineSGHosts; };
    bool getDataChanged() const { return m_dbDataChanged; };

    void ResetCounts() { m_addList.clear(); m_movList.clear(); m_delList.clear(); };

  private:

    struct CheckStruct
    {
        bool check {false};
        QString host;
    };

    using PurgeList = std::vector<std::pair<int, QString> >;
    using FileCheckList = std::map<QString, CheckStruct>;

    void removeOrphans(unsigned int id, const QString &filename);

    void verifyFiles(FileCheckList &files, PurgeList &remove);
    bool updateDB(const FileCheckList &add, const PurgeList &remove);
    bool buildFileList(const QString &directory,
                                        const QStringList &imageExtensions,
                                        FileCheckList &filelist) const;

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
