#ifndef _Myth_Single_Download_h_
#define _Myth_Single_Download_h_

//#include <QObject>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMutex>
#include <QTimer>
#include <QEventLoop>

#include "mythbaseexp.h"

/*
 * MythDownloadManager leaks memory.  For things like the HLS
 * recorder, this leads to aborts in mythbackend when all the memory
 * is exhausted.  Until the leak can be found in MythDownloadManager,
 * avoid the issue by using MythSingleDownload instead.
 */

class MBASE_PUBLIC MythSingleDownload : public QObject
{
   Q_OBJECT

  public:
   MythSingleDownload(void) : m_reply(NULL) { ; }
   ~MythSingleDownload(void) { ; }

   bool DownloadURL(const QString &url, QByteArray *buffer,
		    uint timeout = 30000);
   void Cancel(void);

  private:

    QNetworkAccessManager m_mgr;
    QTimer                m_timer;
    QNetworkReply        *m_reply;
    QMutex                m_lock;
    QMutex                m_replylock;
};

#endif
