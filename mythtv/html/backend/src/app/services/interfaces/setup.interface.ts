export interface HostAddress {
    BackendServerPort:     number;
    BackendStatusPort:     number;
    SecurityPin:           string;
    AllowConnFromAll:      boolean;
    ListenOnAllIps:        boolean;
    BackendServerIP:       string;
    BackendServerIP6:      string;
    AllowLinkLocal:        boolean; 
    BackendServerAddr:     string;
    IsMasterBackend:       boolean;
    MasterServerName:      string;
}

export interface Locale {
    TVFormat:       string;
    VbiFormat:      string;
    FreqTable:      string;
}

export interface Miscellaneous {
    MasterBackendOverride:  boolean;
    DeletesFollowLinks:  boolean;
    TruncateDeletesSlowly:  boolean;
    HDRingbufferSize: number;
    StorageScheduler: string;
    UPNPWmpSource: string;
    MiscStatusScript: string;
    DisableAutomaticBackup: boolean;
    DisableFirewireReset: boolean;
}

export interface Setup {
    General: {
        HostAddress: HostAddress;
        Locale: Locale;
    }
}
