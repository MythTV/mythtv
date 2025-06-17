import { ProgramList } from "./program.interface";

export interface AddDontRecordScheduleRequest {
    ChanId: number;
    StartTime: string; // DateTime
    NeverRecord: boolean;
}

// This supports AddRecordSchedule and UpdateRecordSchedule
// For AddRecordSchedule RecordId is not used
// For UpdateRecordSchedule  ParentId and LastRecorded
// are not used
export interface RecordScheduleRequest {
    RecordId?: number;
    Title: string;
    Subtitle: string;
    Description: string;
    Category: string;
    StartTime: string; // DateTime
    EndTime: string; // DateTime
    SeriesId: string;
    ProgramId: string;
    ChanId: number;
    Station: string;
    FindDay: number;
    FindTime: string; // time
    ParentId?: number;
    Inactive: boolean;
    Season: number;
    Episode: number;
    Inetref: string;
    Type?: string;
    SearchType?: string;
    RecPriority: number;
    PreferredInput: number;
    StartOffset: number;
    EndOffset: number;
    LastRecorded?: string; // dateTime
    DupMethod?: string;
    DupIn?: string;
    NewEpisOnly: boolean;
    Filter: number;
    RecProfile?: string;
    RecGroup?: string;
    StorageGroup?: string;
    PlayGroup?: string;
    AutoExpire: boolean;
    MaxEpisodes: number;
    MaxNewest: boolean;
    AutoCommflag: boolean;
    AutoTranscode: boolean;
    AutoMetaLookup: boolean;
    AutoUserJob1: boolean;
    AutoUserJob2: boolean;
    AutoUserJob3: boolean;
    AutoUserJob4: boolean;
    Transcoder: number;
    AutoExtend?: string;
}

export interface AddRecordedCreditsRequest {
    RecordedId: number;
    json: string;
}

export interface DeleteRecordingRequest {
    RecordedId?: number;
    ChanId?: number;
    StartTime?: string; // dateTime
    ForceDelete?: boolean;
    AllowRerecord?: boolean;
}

export interface UnDeleteRecordingRequest {
    RecordedId?: number;
    ChanId?: number;
    StartTime?: string; // dateTime
}

export interface GetConflictListRequest {
    StartIndex: number;
    Count: number;
    RecordId: number;
}

export interface GetExpiringListRequest {
    StartIndex: number;
    Count: number;
}

export interface GetLastPlayPosRequest {
    RecordedId: number;
    ChanId: number;
    StartTime: string; // dateTime
    OffsetType: string;
}

export interface GetOldRecordedListRequest {
    Descending?: boolean;
    StartIndex?: number;
    Count?: number;
    StartTime?: string; // dateTime
    EndTime?: string; // dateTime
    Title?: string;
    TitleRegex?: string;
    SubtitleRegex?: string;
    SeriesId?: string;
    RecordId?: number;
    Sort?: string;
}

export interface PlayGroupList {
    PlayGroupList: string[];
}

export interface ProgramCategories {
    ProgramCategories: string[];
}

export interface RecGroupList {
    RecGroupList: string[];
}

export interface RecStorageGroupList {
    RecStorageGroupList: string[];
}

export interface GetUpcomingRequest {
    StartIndex?: number;
    Count?:      number;
    ShowAll?:    boolean;
    RecordId?:   number;
    RecStatus?:  string;
    Sort?:       string;
    RecGroup?:   string;
}

export interface UpcomingList {
    ProgramList: ProgramList;
}

export interface GetRecordScheduleRequest {
    RecordId?: number;
    Template?: string;
    RecordedId?: number;
    ChanId?: number;
    StartTime?: string; // dateTime
    MakeOverride?: boolean;
}

export interface GetRecordScheduleListRequest {
    StartIndex?: number;
    Count?: number;
    Sort?: string;
    Descending?: boolean;
}

export interface GetRecordedRequest {
    RecordedId?: number;
    ChanId?: number;
    StartTime?: string; // dateTime
}

export interface GetRecordedListRequest {
    Descending?:    boolean;
    StartIndex?:    number;
    Count?:         number;
    TitleRegEx?:    string;
    RecGroup?:      string;
    StorageGroup?:  string;
    Category?:      string;
    Sort?:          string;
    IgnoreLiveTV?:  boolean;
    IgnoreDeleted?: boolean;
    IncChannel?:    boolean;
    Details?:       boolean;
    IncCast?:       boolean;
    IncArtWork?:    boolean;
    IncRecording?:  boolean;
}

export interface UpdateRecordedMetadataRequest {
    RecordedId:               number;
    AutoExpire?:              boolean;
    BookmarkOffset?:          number;
    BookmarkOffsetType?:      string;
    Damaged?:                 boolean;
    Description?:             string;
    Episode?:                 number;
    Inetref?:                 string;
    OriginalAirDate?:         Date;
    Preserve?:                boolean;
    Season?:                  number;
    Stars?:                   number;
    SubTitle?:                string;
    Title?:                   string;
    Watched?:                 boolean;
    RecGroup?:                string;
}

export interface ManageJobQueueRequest {
    Action:         string;
    JobName:        string;
    JobId?:         number;
    RecordedId:     number;
    JobStartTime?:  Date;
    RemoteHost?:    string;
    JobArgs?:       string;
}

export interface PlayGroup {
    Name:        string;
    TitleMatch:  string;
    SkipAhead:   number;
    SkipBack:    number;
    Jump:        number;
    TimeStretch: number;
}

export interface PowerPriority {
    PriorityName:   string;
    RecPriority:    number;
    SelectClause:   string;
}

export interface PowerPriorityList {
    PowerPriorities : PowerPriority []; 
}

export interface UpdateOldRecordedRequest {
    Chanid:number;
    StartTime:Date;
    Duplicate:boolean;
    Reschedule?:boolean
}

export interface RemoveOldRecordedRequest {
    Chanid:number;
    StartTime:Date;
    Reschedule?:boolean
}
