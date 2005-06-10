#ifndef TRANSCODERCLIENT_H_
#define TRANSCODERCLIENT_H_
/*
	transcoderclient.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's transcoder server

*/

#include "serviceclient.h"

class HttpInResponse;
class MtcpInput;
class JobTracker;

class TranscoderClient : public ServiceClient
{

  public:

    TranscoderClient(
                    MfdInterface *the_mfd,
                    int an_mfd,
                    const QString &l_ip_address,
                    uint l_port
                  );
    ~TranscoderClient();

    void executeCommand(QStringList new_command);
    void handleIncoming();
    void requestRipAudioCd(int collection_id, int playlist_id);
    void requestCancelJob(int job_id);
    void processResponse(HttpInResponse *httpin_response);
    void sendFirstRequest();
    void sendStatusRequest();
    void parseServerInfo(MtcpInput &mtcp_input);
    void parseJobInfo(MtcpInput &mtcp_input);
    void parseJob(MtcpInput &mtcp_input);

  private:

    int generation;    
    QPtrList<JobTracker>    job_list;
};

#endif
