#ifndef EVENTS_H_
#define EVENTS_H_
/*
	events.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    events that get passed to mfdinterface

*/

#include <qevent.h>
#include <qptrlist.h>

#include "mfdcontent.h"

#define MFD_CLIENTLIB_EVENT_DISCOVERY           65432
#define MFD_CLIENTLIB_EVENT_AUDIOPAUSE          65431
#define MFD_CLIENTLIB_EVENT_AUDIOSTOP           65430
#define MFD_CLIENTLIB_EVENT_AUDIOPLAY           65429
#define MFD_CLIENTLIB_EVENT_METADATA            65428
#define MFD_CLIENTLIB_EVENT_AUDIOPLUGIN_EXISTS  65427
#define MFD_CLIENTLIB_EVENT_PLAYLIST_CHECKED    65426
#define MFD_CLIENTLIB_EVENT_SPEAKER_LIST        65425
#define MFD_CLIENTLIB_EVENT_SPEAKER_STREAM      65424
#define MFD_CLIENTLIB_EVENT_TRANSCODER_EXISTS   65423

class SpeakerTracker;

class MfdDiscoveryEvent: public QCustomEvent
{
    //
    //  Sent by discoverythread when it finds a new mfd
    //

  public:
  
    MfdDiscoveryEvent(
                        bool  is_it_lost_or_found,
                        const QString &a_name,
                        const QString &a_hostname, 
                        const QString &an_ip_address,
                        int a_port
                     );

    bool    getLostOrFound();
    QString getName();
    QString getHost();
    QString getAddress();
    int     getPort();
    
  private:
  
    bool    lost_or_found;
    QString name;
    QString hostname;
    QString ip_address;
    int     port;
    
};

class MfdAudioPausedEvent: public QCustomEvent
{
    //
    //  Sent by an audio service client when it discovers that audio it gets
    //  "pause on" or "pause off"
    //
    
  public:
  
    MfdAudioPausedEvent(
                        int which_mfd,
                        bool on_or_off
                       );
    
    int     getMfd();
    bool    getPaused();
    

  private:
  
    int     mfd_id;
    bool    paused_or_not;
};

class MfdAudioStoppedEvent: public QCustomEvent
{
    //
    //  Sent by an audio service client when audio is stopped
    //
    
  public:
  
    MfdAudioStoppedEvent(int which_mfd);

    int     getMfd();
    

  private:
  
    int     mfd_id;
};

class MfdAudioPlayingEvent: public QCustomEvent
{
    //
    //  Sent by an audio service client when audio is stopped
    //
    
  public:
  
    MfdAudioPlayingEvent(
                            int which_mfd,
                            int which_playlist,
                            int index_in_playlist,
                            int which_collection_id,
                            int which_metadata_id,
                            int seconds_elapsed,
                            int numb_channels,
                            int what_bit_rate,
                            int what_sample_frequency
                        );

    int getMfd(){return mfd_id;}
    int getPlaylist(){return playlist;}
    int getPlaylistIndex(){return playlist_index;}
    int getCollectionId(){return collection_id;}
    int getMetadataId(){return metadata_id;}
    int getSeconds(){return seconds;}
    int getChannels(){return channels;}
    int getBitrate(){return bit_rate;}
    int getSampleFrequency(){return sample_frequency;}
    
    

  private:
  
    int mfd_id;
    int playlist;
    int playlist_index;
    int collection_id;
    int metadata_id;
    int seconds;
    int channels;
    int bit_rate;
    int sample_frequency;
};

class MfdMetadataChangedEvent: public QCustomEvent
{
    //
    //  Sent by a metadata client when a new metadata tree is available
    //
    
  public:
  
    MfdMetadataChangedEvent(int which_mfd, MfdContentCollection *new_collection);

    int                getMfd();
    MfdContentCollection* getNewCollection();
    

  private:
  
    int                mfd_id;
    MfdContentCollection *new_mfd_collection;   
};

class MfdAudioPluginExistsEvent: public QCustomEvent
{
    //
    //  Sent by mfdinstance when it discovers an audio plugin in it's mfd
    //
    
  public:
  
    MfdAudioPluginExistsEvent(int which_mfd);

    int                getMfd();

  private:
  
    int                mfd_id;
};

class MfdPlaylistCheckedEvent: public QCustomEvent
{
    //
    //  Sent by the playlist checking thread when it is finished massaging a
    //  content tree
    //
    
  public:
  
    MfdPlaylistCheckedEvent();

};

class MfdSpeakerListEvent: public QCustomEvent
{
    //
    //  Sent by the audio client code whenever the speakers change
    //
    
  public:

    MfdSpeakerListEvent(int which_mfd, QPtrList<SpeakerTracker> *l_speakers);
    ~MfdSpeakerListEvent();
    
    QPtrList<SpeakerTracker>* getSpeakerList(){ return &speakers; }
    int                getMfd();
    
  private:
  
    int                 mfd_id;
    QPtrList<SpeakerTracker> speakers;
};

class MfdSpeakerStreamEvent: public QCustomEvent
{
    //
    //  Sent by audioclient instance when it notices speaker streaming has
    //  been turned on or off
    //
    
  public:
  
    MfdSpeakerStreamEvent(int which_mfd, bool l_on_or_off);
    ~MfdSpeakerStreamEvent();

    int     getMfd();
    bool    getOnOrOff();
    
  private:
  
    int     mfd_id;
    bool    on_or_off;
};

class MfdTranscoderPluginExistsEvent: public QCustomEvent
{
    //
    //  Sent by mfdinstance when it discovers a transcoder plugin in it's mfd
    //
    
  public:
  
    MfdTranscoderPluginExistsEvent(int which_mfd);

    int                getMfd();

  private:
  
    int                mfd_id;
};



#endif
