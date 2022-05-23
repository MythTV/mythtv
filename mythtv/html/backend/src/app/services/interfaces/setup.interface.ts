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
    successCount:           number;
    errorCount:             number;
    MasterBackendOverride:  boolean;
    DeletesFollowLinks:     boolean;
    TruncateDeletesSlowly:  boolean;
    HDRingbufferSize:       number;
    StorageScheduler:       string;
    UPNPWmpSource:          string;
    MiscStatusScript:       string;
    DisableAutomaticBackup: boolean;
    DisableFirewireReset:   boolean;
}

export interface EITScanner {
    successCount:           number;
    errorCount:             number;
    EITTransportTimeout:    number;
    EITCrawIdleStart:       number;
}

export interface ShutWake {
    successCount:               number;
    errorCount:                 number;
    startupCommand:             string,
    blockSDWUwithoutClient:     boolean,
    idleTimeoutSecs:            number,
    idleWaitForRecordingTime:   number,
    StartupSecsBeforeRecording: number,
    WakeupTimeFormat:           string,
    SetWakeuptimeCommand:       string,
    ServerHaltCommand:          string,
    preSDWUCheckCommand:        string
}

export interface BackendWake {
    successCount:                   number;
    errorCount:                     number;
    WOLbackendReconnectWaitTime:    number;
    WOLbackendConnectRetry:         number;
    WOLbackendCommand:              string;
    SleepCommand:                   string;
    WakeUpCommand:                  string;
}

export interface BackendControl {
    successCount:                   number;
    errorCount:                     number;
    BackendStopCommand:             string;
    BackendStartCommand:            string;
}

export interface Setup {
    General: {
        HostAddress: HostAddress;
        Locale: Locale;
    }
}
