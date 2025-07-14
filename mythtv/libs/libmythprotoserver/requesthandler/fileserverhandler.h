#ifndef FILESERVER_H_
#define FILESERVER_H_

#include <QMutex>

#include "libmythbase/filesysteminfo.h"
#include "libmythprotoserver/mythprotoserverexp.h"
#include "libmythprotoserver/mythsocketmanager.h"
#include "libmythprotoserver/requesthandler/fileserverutil.h"
#include "libmythprotoserver/sockethandler/filetransfer.h"

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

    static bool DeleteFile(const QString& filename, const QString& storagegroup);

    static FileSystemInfoList QueryFileSystems(void);
    FileSystemInfoList QueryAllFileSystems(void);

  private:
    static bool HandleQueryFreeSpace(SocketHandler *socket);
    bool HandleQueryFreeSpaceList(SocketHandler *socket);
    bool HandleQueryFreeSpaceSummary(SocketHandler *socket);

    static bool HandleQueryCheckFile(SocketHandler *socket, QStringList &slist);
    static bool HandleQueryFileExists(SocketHandler *socket, QStringList &slist);
    bool HandleQueryFileHash(SocketHandler *socket, QStringList &slist);

    static bool HandleDeleteFile(SocketHandler *socket, QStringList &slist);
    static bool HandleDeleteFile(SocketHandler *socket, const QString& filename,
                                 const QString& storagegroup);
    bool HandleDeleteFile(QString filename, QString storagegroup);
    static bool HandleDeleteFile(DeleteHandler *handler);
    
    bool HandleGetFileList(SocketHandler *socket, QStringList &slist);
    bool HandleFileQuery(SocketHandler *socket, QStringList &slist);
    bool HandleQueryFileTransfer(SocketHandler *socket, QStringList &commands,
                                 QStringList &slist);
    bool HandleDownloadFile(SocketHandler *socket, QStringList &slist);

    static QString LocalFilePath(const QString &path, const QString &wantgroup);
    static void RunDeleteThread(void);

    QMap<int, FileTransfer*>        m_ftMap;
    QReadWriteLock                  m_ftLock;

    QMap<QString, SocketHandler*>   m_fsMap;
    QReadWriteLock                  m_fsLock;

    QMutex                          m_downloadURLsLock;
    QMap<QString, QString>          m_downloadURLs;
};

#endif
