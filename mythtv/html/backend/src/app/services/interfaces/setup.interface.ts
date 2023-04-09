export interface HostAddress {
    successCount:          number;
    errorCount:            number;
    thisHostName:          string;
    BackendServerPort:     number;
    BackendStatusPort:     number;
    SecurityPin:           string;
    AllowConnFromAll:      boolean;
    ListenOnAllIps:        boolean;
    BackendServerIP:       string;
    BackendServerIP6:      string;
    AllowLinkLocal:        boolean; 
    BackendServerAddr:     string;
    // Note : IsMasterBackend is not stored on the DB
    // It is used internally to derive MasterServerName
    IsMasterBackend:       boolean;
    MasterServerName:      string;
}

export interface Locale {
    successCount:   number;
    errorCount:     number;
    TVFormat:       string;
    VbiFormat:      string;
    FreqTable:      string;
}

export interface JobQCommands {
    successCount:                   number;
    errorCount:                     number;
    UserJobDesc:                    string [];
    UserJob:                        string [];
}

export interface Setup {
    General: {
        HostAddress: HostAddress;
        Locale: Locale;
    }
}
