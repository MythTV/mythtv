#ifndef EVENTS_H_
#define EVENTS_H_
/*
	events.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    events that get passed to mfdinterface

*/

#include <qevent.h>

#define MFD_CLIENTLIB_EVENT_DISCOVERY  65432
#define MFD_CLIENTLIB_EVENT_AUDIOPAUSE 65431
#define MFD_CLIENTLIB_EVENT_AUDIOSTOP  65430
#define MFD_CLIENTLIB_EVENT_AUDIOPLAY  65429


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

#endif
