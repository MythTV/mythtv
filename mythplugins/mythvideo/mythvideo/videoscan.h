#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <map>
#include <vector>

#include <QStringList>
#include <QThread>

class MetadataListManager;

class MythUIProgressDialog;

class VideoScannerThread : public QThread
{
    Q_OBJECT
  public:
    VideoScannerThread();
   ~VideoScannerThread();
    void run();
    void SetDirs(const QStringList &dirs);
    void SetProgressDialog(MythUIProgressDialog *dialog);

  private:
    typedef std::vector<std::pair<unsigned int, QString> > PurgeList;
    typedef std::map<QString, bool> FileCheckList;

    void promptForRemoval(unsigned int id, const QString &filename);
    void verifyFiles(FileCheckList &files, PurgeList &remove);
    void updateDB(const FileCheckList &add, const PurgeList &remove);
    void buildFileList(const QString &directory,
                       const QStringList &imageExtensions,
                       FileCheckList &filelist);
    void SendProgressEvent(uint progress, uint total=0, QString messsage="");

    bool m_ListUnknown;
    bool m_RemoveAll;
    bool m_KeepAll;
    QStringList m_directories;

    MetadataListManager *m_dbmetadata;
    MythUIProgressDialog *m_dialog;
};

class VideoScanner : public QObject
{
    Q_OBJECT
  public:
    VideoScanner();
    ~VideoScanner();

    void doScan(const QStringList &dirs);

  signals:
    void finished();

  public slots:
    void finishedScan();

  private:
    VideoScannerThread *m_scanThread;
    bool                m_cancel;
};

#endif
