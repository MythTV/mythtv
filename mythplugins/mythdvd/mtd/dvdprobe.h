/*
	dvdprobe.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    header for dvd probing code (libdvdread)
*/

#ifndef DVDPROBE_H_
#define DVDPROBE_H_

#include <qstring.h>
#include <qptrlist.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>

class DVDAudio
{
    //
    //  A DVDTitle (see below) holds a pointer list
    //  of zero or more DVDAudio objects (one per audio
    //  track)
    //

  public:

    DVDAudio();
    ~DVDAudio();
    
    void    printYourself();
    void    fill(audio_attr_t *audio_attributes);
    QString getAudioString();    

  private:
  
    QString audio_format;
    bool    multichannel_extension;
    QString language;
    QString application_mode;
    QString quantization;
    QString sample_frequency;
    int     channels;
    QString language_extension;

};

class DVDTitle
{
    //
    //  A little "struct" class that holds 
    //  DVD Title informations 
    //  (n.b. a DVD "title" is a logically distinct section
    //  of video, i.e. A movie, a special, a featurette, etc.)
    //

  public:
      
    DVDTitle();
    ~DVDTitle();

    //
    //  Set
    //
    
    void    setChapters(uint a_uint){numb_chapters = a_uint;}
    void    setAngles(uint a_uint){numb_angles = a_uint;}
    void    setTrack(uint a_uint){track_number = a_uint;}
    void    setTime(uint h, uint m, uint s, double fr);
    
    //
    //  Get
    //
    
    uint    getChapters(){return numb_chapters;}
    uint    getAngles(){return numb_angles;}
    uint    getTrack(){return track_number;}
    uint    getPlayLength();
    QString getTimeString();
    uint    getHours(){return hours;}
    uint    getMinutes(){return minutes;}
    uint    getSeconds(){return seconds;}

    void                printYourself();
    void                addAudio(DVDAudio *new_audio_track);
    QPtrList<DVDAudio>* getAudioTracks(){return &audio_tracks;}
        
  private:
  
    uint    numb_chapters;
    uint    numb_angles;
    uint    track_number;
    
    uint    hours;
    uint    minutes;
    uint    seconds;
    double  frame_rate;
    
    QPtrList<DVDAudio>  audio_tracks;

};

class DVDProbe
{
    //
    //  A little class that figures out what's on that
    //  disc in the drive (only DVD's)
    //

  public:
  
    DVDProbe(const QString &dvd_device);
    ~DVDProbe();

    bool                probe();
    QString             getName(){return volume_name;}
    QPtrList<DVDTitle>* getTitles(){return &titles;}
    
  private:

    void         wipeClean();
    bool         first_time;
    QString      device;
    dvd_reader_t *dvd;

    QPtrList<DVDTitle>  titles;
    QString             volume_name;
};

#endif  // dvdprobe_h_

