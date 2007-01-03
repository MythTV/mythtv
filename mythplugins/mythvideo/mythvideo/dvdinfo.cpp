/*
	dvdinfo.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    implementation for dvd "struct"
*/

#include <iostream>
using namespace std;
#include <stdio.h>

#include "dvdinfo.h"

#include <mythtv/mythcontext.h>

DVDAudioInfo::DVDAudioInfo(int track_number, const QString &audio_description)
{
    track = track_number;
    description = audio_description;
}

DVDAudioInfo::~DVDAudioInfo()
{
}

/*
---------------------------------------------------------------------
*/

DVDTitleInfo::DVDTitleInfo()
{
    numb_chapters = 0;
    numb_angles = 0;
    track_number = 0;
    hours = 0;
    minutes = 0;
    seconds = 0;
    audio_tracks.clear();
    audio_tracks.setAutoDelete(true);
    subtitles.clear();
    subtitles.setAutoDelete(true);
    
    is_selected = false;
    selected_quality = -1;

    selected_audio = 1;
    selected_subtitle = -1;
    use_ac3 = gContext->GetNumSetting("MTDac3flag");
    name = "";
}

void DVDTitleInfo::setTime(uint h, uint m, uint s)
{
    hours = h;
    minutes = m;
    seconds = s;
}

uint DVDTitleInfo::getPlayLength()
{
    return seconds + (60 * minutes) + (60 * 60 * hours);
}

QString DVDTitleInfo::getTimeString()
{
    QString a_string;
    a_string.sprintf("%d:%02d:%02d", hours, minutes, seconds);
    return a_string;
}

void DVDTitleInfo::addAudio(DVDAudioInfo *new_audio_track)
{
    audio_tracks.append(new_audio_track);
}

void DVDTitleInfo::addSubTitle(DVDSubTitleInfo *new_subtitle)
{
    subtitles.append(new_subtitle);
}

DVDTitleInfo::~DVDTitleInfo()
{
    audio_tracks.clear();
    subtitles.clear();
}

/*
---------------------------------------------------------------------
*/


DVDInfo::DVDInfo(const QString &new_name)
{
    //
    //  This object just figures out what's on a disc
    //  and tells whoever asks about it.
    //
    
    titles.setAutoDelete(true);
    titles.clear();
    volume_name = new_name;
}

DVDTitleInfo* DVDInfo::getTitle(uint which_one)
{
    DVDTitleInfo *iter;
    for(iter = titles.first(); iter; iter = titles.next())
    {
        if(iter->getTrack() == which_one)
        {
            return iter;
        }
    }
    return NULL;
}


DVDInfo::~DVDInfo()
{
    titles.clear();
}



