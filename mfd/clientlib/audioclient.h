#ifndef AUDIOCLIENT_H_
#define AUDIOCLIENT_H_
/*
	audioclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's audio playing service

*/

#include <qstringlist.h>

#include "serviceclient.h"

class AudioClient : public ServiceClient
{

  public:

    AudioClient(
                MfdInterface *the_mfd,
                int an_mfd,
                const QString &l_ip_address,
                uint l_port
               );


    void handleIncoming();
    void parseFromAudio(QStringList &tokens);
    ~AudioClient();

};

#endif
