#ifndef DVDINFO_H_
#define DVDINFO_H_

#include <QString>
#include <QList>

class DVDSubTitleInfo
{
  public:
    DVDSubTitleInfo(int subtitle_id, const QString &subtitle_name) :
        id(subtitle_id), name(subtitle_name) {}

    int     getID() { return id; }
    QString getName() { return name; }

  private:
    int     id;
    QString name;
};

class DVDAudioInfo
{
  public:
     DVDAudioInfo(int track_number, const QString &audio_description);

    QString getAudioString() { return description; }
    void    setChannels(int a_number) { channels = a_number; }
    int     getChannels() { return channels; }
    int     getTrack() { return track; }

  private:
    QString description;
    int     track;
    int     channels;
};

class DVDTitleInfo
{
  public:
     DVDTitleInfo();
    ~DVDTitleInfo();

    void    setChapters(uint a_uint){numb_chapters = a_uint;}
    void    setAngles(uint a_uint){numb_angles = a_uint;}
    void    setTrack(uint a_uint){track_number = a_uint;}
    void    setTime(uint h, uint m, uint s);
    void    setSelected(bool yes_or_no){is_selected = yes_or_no;}
    void    setQuality(int a_level){selected_quality = a_level;}
    void    setAudio(int which_track){selected_audio = which_track;}
    void    setSubTitle(int which_subtitle){selected_subtitle = which_subtitle;}
    void    setName(QString a_name){name = a_name;}
    void    setInputID(uint a_uint){dvdinput_id = a_uint;}
    void    setAC3(bool y_or_n){use_ac3 = y_or_n;}

    uint    getChapters(){return numb_chapters;}
    uint    getAngles(){return numb_angles;}
    uint    getTrack(){return track_number;}
    uint    getPlayLength();
    QString getTimeString();
    bool    getSelected(){return is_selected;}
    int     getQuality(){return selected_quality;}
    QString getName(){return name;}
    int     getAudio(){return selected_audio;}
    int     getSubTitle(){return selected_subtitle;}
    uint    getInputID(){return dvdinput_id;}
    bool    getAC3(){return use_ac3;}

    void addAudio(DVDAudioInfo *new_audio_track);
    void addSubTitle(DVDSubTitleInfo *new_subtitle);
    QList<DVDAudioInfo *> *getAudioTracks(){return &audio_tracks;}
    QList<DVDSubTitleInfo *> *getSubTitles(){return &subtitles;}
    DVDAudioInfo *getAudioTrack(int which_one)
    {
        return audio_tracks.at(which_one);
    }

  private:
    uint    numb_chapters;
    uint    numb_angles;
    uint    track_number;

    uint    hours;
    uint    minutes;
    uint    seconds;

    QList<DVDAudioInfo *>    audio_tracks;
    QList<DVDSubTitleInfo *> subtitles;

    bool    is_selected;
    int     selected_quality;
    int     selected_audio;
    int     selected_subtitle;
    bool    use_ac3;
    QString name;

    uint    dvdinput_id;
};

class DVDInfo
{
  public:
    DVDInfo(const QString &new_name);
    ~DVDInfo();

    void  addTitle(DVDTitleInfo *new_title){titles.append(new_title);}
    DVDTitleInfo *getTitle(uint which_one);

    QString                 getName(){return volume_name;}
    QList<DVDTitleInfo *> *getTitles(){return &titles;}

  private:
    QList<DVDTitleInfo *> titles;
    QString volume_name;
};

#endif  // dvdinfo_h_
