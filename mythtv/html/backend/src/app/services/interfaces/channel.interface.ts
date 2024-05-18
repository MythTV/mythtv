import { ScheduleOrProgram } from "./program.interface";

export interface Channel {
    ATSCMajorChan:      number;
    ATSCMinorChan:      number;
    CallSign:           string;
    ChanFilters:        string;
    ChanId:             number;
    ChanNum:            string; // number sent as string
    ChannelGroups:      string; // null in sample data
    ChannelName:        string;
    CommFree:           boolean;
    CommMethod:         number;
    DefaultAuth:        string;
    ExtendedVisible:    string;
    FineTune:           number;
    Format:             string;
    FrequencyId:        string; // null in sample data
    IconURL:            string;
    Icon:               string;
    InputId:            number;
    Inputs:             string; // null in sample data
    MplexId:            number;
    Programs:           ScheduleOrProgram[];
    RecPriority:        number;
    ServiceId:          number;
    ServiceType:        number;
    SourceId:           number;
    TimeOffset:         number;
    UseEIT:             boolean;
    Visible:            boolean;
    XMLTVID:            string;
}

export interface CommMethod {
    CommMethod:         string;
    LocalizedName:      string;
}

export interface CommMethodList {
    CommMethodList: {
        CommMethods: CommMethod[];
    }
}

export interface DBChannelRequest {
    ATSCMajorChan?:     number;
    ATSCMinorChan?:     number;
    CallSign?:          string;
    ChannelID:          number;
    ChannelNumber?:     string; // number sent as string
    ChannelName?:       string;
    CommMethod?:        number;
    DefaultAuthority?:  string;
    ExtendedVisible?:   string;
    Format?:            string;
    FrequencyID?:       string; // null in sample data
    Icon?:              string;
    MplexID?:           number;
    RecPriority?:       number;
    ServiceID?:         number;
    ServiceType?:       number;
    SourceID?:          number;
    TimeOffset?:        number;
    UseEIT?:            boolean;
    Visible?:           boolean;
    XMLTVID?:           string;
}

export interface FetchChannelsFromSourceRequest {
    SourceId:           number;
    CardId:             number;
    WaitForFinish:      boolean;
}

// All parameters are optional.
export interface GetChannelInfoListRequest {
    SourceID?:           number;
    ChannelGroupID?:     number;
    StartIndex?:         number;
    Count?:              number;
    OnlyVisible?:        boolean;
    Details?:            boolean;
    OrderByName?:        boolean;
    GroupByCallsign?:    boolean;
    OnlyTunable?:        boolean;
}

export interface GetVideoMultiplexListRequest {
    SourceID:           number;
    StartIndex?:        number;
    Count?:             number;
}

export interface UpdateVideoSourceRequest {
    SourceID:           number;
    SourceName:         string;
    Grabber:            string;
    UserId:             string;
    FreqTable:          string;
    LineupId:           string;
    Password:           string;
    UseEIT:             boolean;
    ConfigPath:         string;
    NITId:              number;
    BouquetId:          number;
    RegionId:           number;
    ScanFrequency:      number;
    LCNOffset:          number;
}

export interface ChannelScanRequest {
    CardId:                 number;
    DesiredServices:        string;
    FreeToAirOnly:          boolean;
    ChannelNumbersOnly:     boolean;
    CompleteChannelsOnly:   boolean;
    FullChannelSearch:      boolean;
    RemoveDuplicates:       boolean;
    AddFullTS:              boolean;
    TestDecryptable:        boolean;
    ScanType:               string;
    FreqTable:              string;
    Modulation:             string;
    FirstChan:              string;
    LastChan:               string;
    ScanId:                 number;
    IgnoreSignalTimeout:    boolean;
    FollowNITSetting:       boolean;
    MplexId:                number;
    Frequency:              number;
    Bandwidth:              string;
    Polarity:               string;
    SymbolRate:             string;
    Inversion:              string;
    Constellation:          string;
    ModSys:                 string;
    CodeRateLP:             string;
    CodeRateHP:             string;
    FEC:                    string;
    TransmissionMode:       string;
    GuardInterval:          string;
    Hierarchy:              string;
    RollOff:                string;
}

export interface ChannelScanStatus {
    CardId:                 number;
    Status:                 string;
    SignalLock:             boolean;
    Progress:               number;
    SignalNoise:            number;
    SignalStrength:         number;
    StatusLog:              string;
    StatusText:             string;
    StatusTitle:            string;
    DialogMsg:              string;
    DialogInputReq:         boolean;
    DialogButtons:          string[];
}

export interface ScanDialogResponse {
    CardId:                 number;
    DialogString:            string;
    DialogButton:           number;
}

export interface Scan {
    ScanId:                 number;
    CardId:                 number;
    SourceId:               number;
    Processed:              boolean;
    ScanDate:               string;       // Date
}

export interface ChannelRestoreData {
    NumChannels:            number;
    NumXLMTVID:             number;
    NumIcon:                number;
    NumVisible:             number;
}
