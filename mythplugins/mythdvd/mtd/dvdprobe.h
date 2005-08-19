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

#include <dvdread/ifo_types.h>

class DVDSubTitle
{
    //
    //  A DVDTitle (below) holds zero or more
    //  of these objects, each describing available
    //  subtitles.
    //
  
  public:
    
    DVDSubTitle(int subtitle_id, const QString &a_language);

    void    setName(const QString &a_name){name = a_name;}
    QString getLanguage(){return language;}
    QString getName(){return name;}
    int     getID(){return id;}
    
  private:
  
    int     id;
    QString language;
    QString name;
};

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
    int     getChannels(){return channels;}
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
    void    setAR(uint n, uint d, const QString &ar);
    void    setSize(uint h, uint v){hsize = h; vsize = v;}
    void    setLBox(bool yes_or_no){letterbox = yes_or_no;}
    void    setVFormat(const QString &a_string){video_format = a_string;}    
    void    determineInputID();

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
    uint    getInputID(){return dvdinput_id;}
    

    void                   printYourself();
    void                   addAudio(DVDAudio *new_audio_track);
    QPtrList<DVDAudio>*    getAudioTracks(){return &audio_tracks;}
    void                   addSubTitle(DVDSubTitle *new_subitle);
    QPtrList<DVDSubTitle>* getSubTitles(){return &subtitles;}        
    
  private:
  
    uint    numb_chapters;
    uint    numb_angles;
    uint    track_number;
    
    uint    hours;
    uint    minutes;
    uint    seconds;
    
    uint    hsize;
    uint    vsize;
    double  frame_rate;
    int     fr_code;
    uint    ar_numerator;
    uint    ar_denominator;
    QString aspect_ratio;    
    bool    letterbox;
    QString video_format;
    uint    dvdinput_id;
    
    QPtrList<DVDAudio>    audio_tracks;
    QPtrList<DVDSubTitle> subtitles;
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
    DVDTitle*           getTitle(uint which_one);
    
  private:

    void         wipeClean();
    bool         first_time;
    QString      device;
    dvd_reader_t *dvd;

    QPtrList<DVDTitle>  titles;
    QString             volume_name;
};

#endif  // dvdprobe_h_

