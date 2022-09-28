export interface VideoSource {
    Id:             number;
    SourceName:     string;
    Grabber:        string;
    UserId:         string;
    FreqTable:      string;
    LineupId:       string;
    Password:       string;
    UseEIT:         boolean;
    ConfigPath:     string;
    NITId:          number;
    BouquetId:      number;
    RegionId:       number;
    ScanFrequency:  number;
    LCNOffset:      number;
}

export interface VideoSourceList {
    VideoSourceList: {
        AsOf:       string; // dateTime
        Version:    string;
        ProtoVer:   string;
        VideoSources: VideoSource[];
    }
}

export interface Grabber {
Program:            string;
    DisplayName:    string;
}

export interface GrabberList {
    GrabberList: {
        Grabbers:   Grabber[];
    }
}

export interface FreqTableList {
    FreqTableList: string[];
}
