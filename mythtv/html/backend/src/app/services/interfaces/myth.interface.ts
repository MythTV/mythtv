export interface MythHostName {
    String: string;  // That's what the service returns as the key
}
export interface MythTimeZone {
    TimeZoneInfo: {
        CurrentDateTime:    string;
        TimeZoneID:         string;
        UTCOffset:          number;
    }
}
