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
    MYTHCONFDIR:    string;
}

export interface LogInfo {
    LogArgs:        string;
}

export interface BackendInfo {
    Build:          BuildInfo;
    Env:            EnvInfo;
    Log:            LogInfo;
}

export interface Backend {
    IP:             string;
    Name:           string;
    Type:           string;
}

