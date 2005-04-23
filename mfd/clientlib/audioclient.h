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

class SpeakerTracker
{

  public:

    SpeakerTracker(int l_id, const QString yes_or_no);
    ~SpeakerTracker();
    
    void    markForDeletion(bool b){ marked_for_deletion = b; }
    bool    markedForDeletion(){ return marked_for_deletion; }
    bool    possiblyUnmarkForDeletion(const QString &id_string, const QString &inuse_string);
    int     getId(){ return id; }
    QString getInUse(){ if (in_use) return QString("yes"); return QString("no"); }
 
  private:

    int     id;
    QString name;   
    bool    in_use;
    bool    marked_for_deletion;
};

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
    void syncSpeakerList(QStringList &tokens);
    ~AudioClient();

  private:
  
    QPtrList<SpeakerTracker>    speakers;

};

#endif
