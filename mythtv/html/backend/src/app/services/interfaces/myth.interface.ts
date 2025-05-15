import { StorageGroupDirList } from "./storagegroup.interface";

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
    WOLEnabled?:        boolean;
    WOLReconnect?:      number;
    WOLRetry?:          number;
    WOLCommand?:        string;
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
    HostName?:   string;
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

export interface GetStorageGroupDirsRequest {
    GroupName?: string;
    HostName?:  string;
}

export interface GetStorageGroupDirsResponse {
    StorageGroupDirList:  StorageGroupDirList;
}

export interface AddStorageGroupDirRequest {
    GroupName:  string;
    DirName:    string;
    HostName:   string;
}

export interface CheckDatabaseRequest {
    Repair:         boolean;
}

export interface MapOfString {
    [key: string]:  string;
}

export interface SettingList {
    SettingList: {
        HostName:       string;
        Settings:       MapOfString;
    }
}

export interface ManageDigestUserRequest {
    Action:         string; // Must be: Add, Remove, or ChangePassword
    UserName:       string;
    Password?:      string;
    NewPassword?:   string; // Required on ChangePassword
}

export interface ManageUrlProtectionRequest {
    Services:       string;
    AdminPassword:  string;
}

export interface RemoveStorageGroupRequest {
    GroupName:      string;
    DirName:        string;
    HostName:       string;
}

export interface TestDBSettingsRequest {
    HostName:       string;
    UserName:       string;
    Password:       string;
    DBName:         string;
    dbPort:         number;
}