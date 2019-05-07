#ifndef FILESERVER_H_
#define FILESERVER_H_

#include <QMutex>

#include "mythsocketmanager.h"
#include "filesysteminfo.h"
#include "mythprotoserverexp.h"

#include "sockethandler/filetransfer.h"
#include "requesthandler/fileserverutil.h"

class PROTOSERVER_PUBLIC FileServerHandler : public SocketRequestHandler
{
    Q_OBJECT
  public:
    bool HandleAnnounce(MythSocket *socket, QStringList &commands,
                        QStringList &slist) override; // SocketRequestHandler
    bool HandleQuery(SocketHandler *socket, QStringList &commands,
                     QStringList &slist) override; // SocketRequestHandler
    QString GetHandlerName(void) override // SocketRequestHandler
        { return "FILETRANSFER"; }

    void connectionAnnounced(MythSocket *socket, QStringList &commands,
                             QStringList &slist) override; // SocketRequestHandler
    void connectionClosed(MythSocket *socket) override; // SocketRequestHandler

    bool DeleteFile(QString filename, QString storagegroup);

    QList<FileSystemInfo> QueryFileSystems(void);
    QList<FileSystemInfo> QueryAllFileSystems(void);

  private:
    bool HandleQueryFreeSpace(SocketHandler *socket);
    bool HandleQueryFreeSpaceList(SocketHandler *socket);
    bool HandleQueryFreeSpaceSummary(SocketHandler *socket);

    bool HandleQueryCheckFile(SocketHandler *socket, QStringList &slist);
    bool HandleQueryFileExists(SocketHandler *socket, QStringList &slist);
    bool HandleQueryFileHash(SocketHandler *socket, QStringList &slist);

    bool HandleDeleteFile(SocketHandler *socket, QStringList &slist);
    bool HandleDeleteFile(SocketHandler *socket, const QString& filename,
                          const QString& storagegroup);
    bool HandleDeleteFile(QString filename, QString storagegroup);
    bool HandleDeleteFile(DeleteHandler *handler);
    
    bool HandleGetFileList(SocketHandler *socket, QStringList &slist);
    bool HandleFileQuery(SocketHandler *socket, QStringList &slist);
    bool HandleQueryFileTransfer(SocketHandler *socket, QStringList &commands,
                                 QStringList &slist);
    bool HandleDownloadFile(SocketHandler *socket, QStringList &slist);

    QString LocalFilePath(const QString &path, const QString &wantgroup);
    void RunDeleteThread(void);

    QMap<int, FileTransfer*>        m_ftMap;
    QReadWriteLock                  m_ftLock;

    QMap<QString, SocketHandler*>   m_fsMap;
    QReadWriteLock                  m_fsLock;

    QMutex                          m_downloadURLsLock;
    QMap<QString, QString>          m_downloadURLs;
};

#endif
