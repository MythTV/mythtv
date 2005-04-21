#ifndef SPEAKERS_H_
#define SPEAKERS_H_
/*
	speakers.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the speakers plugin
*/

#include "mfd_plugin.h"

class RtspIn;

class Speakers: public MFDServicePlugin
{

  public:

    Speakers(MFD *owner, int identity);
    ~Speakers();

    void run();
    void doSomething(const QStringList &tokens, int socket_identifier);
    void openStream(QString stream_url);
    void closeStream();
    void announceStatus();

  private:
  
    QString current_url;

    QMutex   rtsp_in_mutex;
    RtspIn  *rtsp_in;
};

#endif  // speakers_h_
