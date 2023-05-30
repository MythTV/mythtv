import { ArtworkInfoList } from "./artwork.interface";
import { CastMemberList } from "./castmember.interface";
import { Channel } from "./channel.interface";
import { Recording } from "./recording.interface";

export interface ScheduleOrProgram {
    chanId: number;
    Airdate:            Date;
    Artwork:            ArtworkInfoList;
    AudioPropNames:     string;
    AudioProps:         number;
    Cast:               CastMemberList;
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

export interface ProgramList {
    StartIndex:         number;
    Count:              number;
    TotalAvailable:     number;
    AsOf:               string; // DateTime
    Version:            string;
    ProtoVer:           string;
    Programs:           ScheduleOrProgram[];
}
