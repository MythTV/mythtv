#ifndef HTTPSTATUS_H_
#define HTTPSTATUS_H_

#include <qserversocket.h>
#include <qsocket.h>
#include <qdom.h>
#include <qdatetime.h> 

class MainServer;

class HttpStatus : public QServerSocket
{
    Q_OBJECT
  public:
    HttpStatus(MainServer *parent, int port);

    void newConnection(int socket);

  private slots:
    void readClient();
    void discardClient();

  private:

    QDateTime GetDateTime       ( QString sDate );
    void      PrintStatus       ( QSocket *socket, QDomDocument *pDoc );
    int       PrintEncoderStatus( QTextStream &os, QDomElement encoders );
    int       PrintScheduled    ( QTextStream &os, QDomElement scheduled );
    int       PrintJobQueue     ( QTextStream &os, QDomElement jobs );
    int       PrintMachineInfo  ( QTextStream &os, QDomElement info );

#if USING_DVB
    int       PrintDVBStatus    ( QTextStream &os, QDomElement info );
#endif

  private:
    MainServer *m_parent;
};

#endif
