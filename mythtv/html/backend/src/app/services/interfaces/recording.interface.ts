export interface Recording {
    DupInType:      number;
    DupMethod:      number;
    EncoderId:      number;
    EncoderName:    string;
    EndTs:          string;
    FileName:       string;
    FileSize:       number;
    HostName:       string;
    LastModified:   string;
    PlayGroup:      string;
    Priority:       number;
    Profile:        string;
    RecGroup:       string;
    RecType:        number;
    RecordId:       number;
    RecordedId:     number;
    RecTypeStatus:  string;
    StartTs:        string;
    Status:         number;
    StatusName:     string;
    StorageGroup:   string;
}

export interface RecRuleFilter {
    Id:             number;
    Description:    string;
}

export interface RecRuleFilterList {
    StartIndex:     number;
    Count:          number;
    TotalAvailable: number;
    AsOf:           string; // dateTime
    Version:        string;
    ProtoVer:       string;
    RecRuleFilters: RecRuleFilter[];
}

export interface RecRule {
    Id:             number;
    ParentId:       number;
    Inactive:       boolean;
    Title:          string;
    SubTitle:       string;
    Description:    string;
    Season:         number;
    Episode:        number;
    Category:       string;
    StartTime:      string; // dateTime
    EndTime:        string; // dateTime
    SeriesId:       string;
    ProgramId:      string;
    Inetref:        string;
    ChanId:         number;
    CallSign:       string;
    FindDay:        number;
    FindTime:       string; // time
    Type:           string;
    SearchType:     string;
    RecPriority:    number;
    PreferredInput: number;
    StartOffset:    number;
    EndOffset:      number;
    DupMethod:      string;
    DupIn:          string;
    NewEpisOnly:    boolean;
    Filter:         number;
    RecProfile:     string;
    RecGroup:       string;
    StorageGroup:   string;
    PlayGroup:      string;
    AutoExpire:     boolean;
    MaxEpisodes:    number;
    MaxNewest:      boolean;
    AutoCommflag:   boolean;
    AutoTranscode:  boolean;
    AutoMetaLookup: boolean;
    AutoUserJob1:   boolean;
    AutoUserJob2:   boolean;
    AutoUserJob3:   boolean;
    AutoUserJob4:   boolean;
    Transcoder:     number;
    NextRecording:  string; // dateTime
    LastRecorded:   string; // dateTime
    LastDeleted:    string; // dateTime
    AverageDelay:   number;
    AutoExtend:     string;
}

export interface RecRuleList {
    StartIndex:     number;
    Count:          number;
    TotalAvailable: number;
    AsOf:           string; // dateTime
    Version:        string;
    ProtoVer:       string;
    RecRules:       RecRule[];
}
