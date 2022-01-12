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

export interface Version {
    Version:            string;
    Branch:             string;
    Protocol:           number;
    Binary:             string;
    Schema:             number;
}

export interface Database {
    Host:               string;  // can be hostname or ip address
    Port:               number;  // default is 3306
    UserName:           string;
    Password:           string;
    Name:               string;  // default is mythconverg
    Ping:               boolean;
    Type:               string;
    LocalEnabled:       boolean;
    LocalHostName:      string;
    DoTest:             boolean; // will test connection if true
}

export interface WOL {
    Enabled:            boolean;
    Reconnect:          number;
    Retry:              number;
    Command:            string;
}

export interface MythConnectionInfo {
    ConnectionInfo: {
        Version:            Version;
        Database:           Database;
        WOL:                WOL;
    }
}

export interface GetSettingRequest {
    HostName:   string;
    Key:        string;
    Default?:   string;
}

export interface GetSettingResponse {
    String:     string;
}

export interface PutSettingRequest {
    HostName:   string;
    Key:        string;
    Value:      string;
}

export interface PutSettingResponse {
    bool:       boolean;
}
