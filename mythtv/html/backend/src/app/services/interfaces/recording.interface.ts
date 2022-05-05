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
    StartTs:        string;
    Status:         number;
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
