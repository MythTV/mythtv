import { Observable } from "rxjs";
import { GetSettingResponse } from "./myth.interface";

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

export interface JobQBackend {
    successCount:                   number;
    errorCount:                     number;
    JobQueueMaxSimultaneousJobs:    number;
    JobQueueCheckFrequency:         number;
    JobQueueWindowStart:            Date;
    JobQueueWindowStartObs:         Observable<GetSettingResponse>;
    JobQueueWindowEnd:              Date;
    JobQueueWindowEndObs:           Observable<GetSettingResponse>;
    JobQueueCPU:                    string;
    JobAllowMetadata:               boolean;
    JobAllowCommFlag:               boolean;
    JobAllowTranscode:              boolean;
    JobAllowPreview:                boolean;
    JobAllowUserJob1:               boolean;
    JobAllowUserJob2:               boolean;
    JobAllowUserJob3:               boolean;
    JobAllowUserJob4:               boolean;
}

export interface JobQCommands {
    successCount:                   number;
    errorCount:                     number;
    UserJobDesc1:                   string;
    UserJobDesc2:                   string;
    UserJobDesc3:                   string;
    UserJobDesc4:                   string;
}

export interface JobQGlobal {
    successCount:                   number;
    errorCount:                     number;
    JobsRunOnRecordHost:            boolean;
    AutoCommflagWhileRecording:     boolean;
    JobQueueCommFlagCommand:        string;
    JobQueueTranscodeCommand:       string;
    AutoTranscodeBeforeAutoCommflag:boolean;
    SaveTranscoding:                boolean;
}


export interface Setup {
    General: {
        HostAddress: HostAddress;
        Locale: Locale;
    }
}
