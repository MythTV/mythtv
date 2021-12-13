import { Channel } from "./channel.interface";
import { Recording } from "./recording.interface";

export interface ArtworkInfo {
    // no data
}

export interface Artwork {
    ArtworkInfos: ArtworkInfo[];
}

export interface CastMember {
    // no data
}

export interface Cast {
    CastMembers:        CastMember[];
}

export interface ScheduleOrProgram {
    Airdate:            string;
    Artwork:            Artwork;
    AudioPropNames:     string;
    AudioProps:         number;
    Cast:               Cast;
    CatType:            string;
    Category:           string;
    Channel:            Channel;
    Description:        string;
    EndTime:            string;
    Episode:            number;
    FileName:           string;
    FileSize:           number;
    HostName:           string;
    Inetref:            string;
    LastModified:       string;
    ProgramFlagNames:   string;
    ProgramFlags:       number;
    ProgramId:          string;
    Recording:          Recording;
    Repeat:             boolean;
    Season:             number;
    SeriesId:           string;
    Stars:              number;
    StartTime:          string;
    SubPropNames:       string;
    SubProps:           number;
    SubTitle:           string;
    Title:              string;
    TotalEpisodes:      number;
    VideoPropNames:     string;
    VideoProps:         number;
}
