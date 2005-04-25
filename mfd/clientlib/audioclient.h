#ifndef AUDIOCLIENT_H_
#define AUDIOCLIENT_H_
/*
	audioclient.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's audio playing service

*/

#include <qstringlist.h>

#include "serviceclient.h"
#include "speakertracker.h"

class AudioClient : public ServiceClient
{

  public:

    AudioClient(
                MfdInterface *the_mfd,
                int an_mfd,
                const QString &l_ip_address,
                uint l_port
               );


    void playTrack(int container, int id);
    void playList(int container, int id, int index);
    void stopAudio();
    void pauseAudio(bool y_or_n);
    void seekAudio(int how_much);
    void nextAudio();
    void prevAudio();
    void handleIncoming();
    void parseFromAudio(QStringList &tokens);
    void executeCommand(QStringList new_command);
    void askForStatus();
    void toggleSpeakers(const QString &full_command);
    void syncSpeakerList(QStringList &tokens);
    void nameSpeakers(QStringList &tokens);
    ~AudioClient();

  private:
  
    QPtrList<SpeakerTracker>    speakers;

};

#endif
