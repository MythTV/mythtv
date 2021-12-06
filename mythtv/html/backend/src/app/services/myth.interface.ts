export interface MythHostName {
    HostName: string;
}
export interface MythTimeZone {
    TimeZoneInfo: {
        CurrentDateTime:    string;
        TimeZoneID:         string;
        UTCOffset:          number;
    }
}
