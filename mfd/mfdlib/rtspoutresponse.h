#ifndef RTSPOUTRESPONSE_H_
#define RTSPOUTRESPONSE_H_
/*
	rtspoutresponse.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an object to build rtsp responses and send them
*/

#include <qdict.h>

#include "httpheader.h"
#include "clientsocket.h"

class MFDRtspPlugin;

class RtspOutResponse
{

  public:

    RtspOutResponse(MFDRtspPlugin *owner, int cseq);
    ~RtspOutResponse();

    void addHeader(const QString &filed, const QString &value);
    void addText(std::vector<char> *buffer, QString text_to_add);
    void send(MFDServiceClientSocket *which_client);
    bool sendBlock(MFDServiceClientSocket *which_client, std::vector<char> &block_to_send);

    void    log(const QString &log_message, int verbosity);
    void    warning(const QString &warning_message);
    
  private:
    
    MFDRtspPlugin     *parent; 
    int               command_sequence;
    QDict<HttpHeader> headers;
    int               status_code;
    QString           status_string;
};

#endif

