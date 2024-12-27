export interface BuildInfo {
    Version:        string;
    LibX264:        boolean;
    LibDNS_SD:      boolean;
}

export interface EnvInfo {
    LANG:           string;
    LCALL:          string;
    LCCTYPE:        string;
    HOME:           string;
    USER:           string;
    MYTHCONFDIR:    string;
    HttpRootDir:    string;
    SchedulingEnabled: boolean;
    IsDatabaseIgnored: boolean;
    DBTimezoneSupport: boolean;
    WebOnlyStartup:  string;
}

export interface LogInfo {
    LogArgs:        string;
}

export interface BackendInfo {
    BackendInfo : {
        Build:          BuildInfo;
        Env:            EnvInfo;
        Log:            LogInfo;
    }
}

export interface Backend {
    IP:             string;
    Name:           string;
    Type:           string;
}

