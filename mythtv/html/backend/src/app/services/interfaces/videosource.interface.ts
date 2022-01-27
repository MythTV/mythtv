export interface VideoSource {
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