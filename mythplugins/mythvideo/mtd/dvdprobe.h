/*
    dvdprobe.h

    (c) 2003 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project

    header for dvd probing code (libdvdread)
*/

#ifndef DVDPROBE_H_
#define DVDPROBE_H_

#include <inttypes.h>

#ifndef UINT8_MAX
#define UINT8_MAX
#define UINT16_MAX
#define INT32_MAX
#define MAXDEFS
#endif

#include <mythtv/dvdread/ifo_types.h>
#include <mythtv/dvdread/ifo_read.h>
#include <mythtv/dvdread/dvd_reader.h>
#include <mythtv/dvdread/nav_read.h>

#ifdef MAXDEFS
#undef UINT8_MAX
#undef UINT16_MAX
#undef INT32_MAX
#endif

// Qt headers
#include <QString>
#include <QList>

class DVDSubTitle
{
    //
    //  A DVDTitle (below) holds zero or more
    //  of these objects, each describing available
    //  subtitles.
    //

  public:
    DVDSubTitle(int subtitle_id, const QString &a_language);

    void    SetName(const QString &a_name) { name = a_name; }
    QString GetLanguage(void) const { return language; }
    QString GetName(void) const { return name; }
    int     GetID(void) const { return id; }

  private:
    int     id;
    QString language;
    QString name;
};
typedef QList<DVDSubTitle> DVDSubTitleList;

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

    void    printYourself(void) const;
    void    fill(audio_attr_t *audio_attributes);
    int     GetChannels(void) const { return channels; }
    QString GetAudioString(void) const;

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
typedef QList<DVDAudio>    DVDAudioList;

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

    void    SetChapters(uint a_uint) { numb_chapters = a_uint; }
    void    SetAngles(uint a_uint) { numb_angles = a_uint; }
    void    SetTrack(uint a_uint) { track_number = a_uint; }
    void    SetTime(uint h, uint m, uint s, double fr);
    void    SetAR(uint n, uint d, const QString &ar);
    void    SetSize(uint h, uint v) { hsize = h; vsize = v; }
    void    SetLBox(bool yes_or_no) { letterbox = yes_or_no; }
    void    SetVFormat(const QString &a_string) { video_format = a_string; }
    void    determineInputID();

    //
    //  Get
    //

    uint    GetChapters(void)            const { return numb_chapters; }
    uint    GetAngles(void)              const { return numb_angles; }
    uint    GetTrack(void)               const { return track_number; }
    uint    GetHours(void)               const { return hours; }
    uint    GetMinutes(void)             const { return minutes; }
    uint    GetSeconds(void)             const { return seconds; }
    uint    GetInputID(void)             const { return dvdinput_id; }
    DVDAudioList    GetAudioTracks(void) const { return audio_tracks; }
    DVDSubTitleList GetSubTitles(void)   const { return subtitles; }
    uint    GetPlayLength(void) const;
    QString GetTimeString(void) const;
    bool    IsValid(void) const;

    void    AddAudio(const DVDAudio &new_audio_track);
    void    AddSubTitle(const DVDSubTitle &new_subitle);

    void    printYourself(void) const;

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

    DVDAudioList    audio_tracks;
    DVDSubTitleList subtitles;
};
typedef QList<DVDTitle> DVDTitleList;

class DVDProbe
{
    //
    //  A little class that figures out what's on that
    //  disc in the drive (only DVD's)
    //

  public:
    DVDProbe(const QString &dvd_device);
    ~DVDProbe();

    bool          Probe(void);
    QString       GetName(void)   const { return volume_name; }
    DVDTitleList  GetTitles(void) const { return titles;      }
    DVDTitle      GetTitle(uint which_one) const;

  private:
    void          Reset(void);

  private:
    dvd_reader_t *dvd;
    bool          first_time;
    QString       device;
    QString       volume_name;
    DVDTitleList  titles;
};

#endif  // dvdprobe_h_
