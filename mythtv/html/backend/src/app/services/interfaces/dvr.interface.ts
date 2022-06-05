import { ProgramList } from "./program.interface";

export interface AddDontRecordScheduleRequest {
    ChanId:                 number;
    StartTime:              string; // DateTime
    NeverRecord:            boolean;
}

export interface AddRecordScheduleRequest {
    Title:                  string;
    Subtitle:               string;
    Description:            string;
    Category:               string;
    StartTime:              string; // DateTime
    EndTime:                string; // DateTime
    SeriesId:               string;
    ProgramId:              string;
    ChanId:                 number;
    Station:                string;
    FindDay:                number;
    FindTime:               string; // time
    ParentId:               number;
    Inactive:               boolean;
    Season:                 number;
    Episode:                number;
    InetRef:                string;
    Type?:                  string;
    SearchType?:            string;
    RecPriority:            number;
    PreferredInput:         number;
    StartOffset:            number;
    EndOffset:              number;
    LastRecorded:           string; // dateTime
    DupMethod?:             string;
    DupIn?:                 string;
    NewEpisOnly:            boolean;
    Filter:                 number;
    RecProfile?:            string;
    RecGroup?:              string;
    StorageGroup?:          string;
    PlayGroup?:             string;
    AutoExpire:             boolean;
    MaxEpisodes:            number;
    MaxNewest:              boolean;
    AutoCommflag:           boolean;
    AutoTranscode:          boolean;
    AutoMetaLookup:         boolean;
    AutoUserJob1:           boolean;
    AutoUserJob2:           boolean;
    AutoUserJob3:           boolean;
    AutoUserJob4:           boolean;
    Transcoder:             number;
}

export interface AddRecordedCreditsRequest {
    RecordedId:             number;
    json:                   string;
}

export interface DeleteRecordingRequest {
    RecordedId:             number;
    ChanId:                 number;
    StartTime:              string; // dateTime
    ForceDelete:            boolean;
    AllowRerecord:          boolean;
}

export interface GetConflictListRequest {
    StartIndex:             number;
    Count:                  number;
    RecordId:               number;
}

export interface GetExpiringListRequest {
    StartIndex:             number;
    Count:                  number;
}

export interface GetLastPlayPosRequest {
    RecordedId:             number;
    ChanId:                 number;
    StartTime:              string; // dateTime
    OffsetType:             string;
}

export interface GetOldRecordedListRequest {
    Descending:             boolean;
    StartIndex:             number;
    Count:                  number;
    StartTime:              string; // dateTime
    EndTime:                string; // dateTime
    Title:                  string;
    SeriesId:               string;
    RecordId:               number;
    Sort:                   string;
}

export interface GetPlayGroupListResponse {
    PlayGroupList:          String[];
}

export interface GetProgramCategoriesResponse {
    ProgramCategories:      String[];
}

export interface GetRecGroupListResponse {
    RecGroupList:           String[];
}

export interface GetRecStorageGroupListResponse {
    RecStorageGroupList:    String[];
}

export interface GetUpcomingListResponse {
    ProgramList:            ProgramList;
}

export interface GetRecordScheduleRequest {
    RecordId:               number;
    Template:               string;
    RecordedId:             number;
    ChanId:                 number;
    StartTime:              string; // dateTime
    MakeOverride:           boolean;
}

export interface GetRecordScheduleListRequest {
    StartIndex:             number;
    Count:                  number;
    Sort:                   string;
    Descending:             boolean;
}

export interface GetRecordedRequest {
    RecordedId:             number;
    ChanId:                 number;
    StartTime:              string; // dateTime
}
