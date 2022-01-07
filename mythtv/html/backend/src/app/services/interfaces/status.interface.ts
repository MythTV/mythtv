import { Encoder } from "./encoder.interface";
import { JobQueueJob } from "./jobqueue.interface";
import { ScheduleOrProgram } from "./schedule.interface";

export interface Backend {
    IP:             string;
    Name:           string;
    Type:           string;
}

export interface Frontend {
    IP:             string;
    Name:           string;
    OnLine:         boolean;
    Port:           number;
}

export interface StorageGroup {
    Deleted:        number; // Boolean?
    Directory:      string;
    Expirable:      number; // Boolean?
    Free:           number;
    Id:             string;
    LiveTV:         number;
    Total:          number;
    Used:           number;
}

export interface MachineInfo {
    GuideDays:      number;
    GuideEnds:      string;
    GuideNext:      string;
    GuideStart:     string;
    GuideStatus:    string;
    GuideThru:      string;
    LoadAvg1:       string;
    LoadAvg2:       string;
    LoadAvg3:       string;
    StorageGroups:  StorageGroup[];
}

export interface BackendStatus {
    AsOf:           string;
    Backends:       Backend[];
    Encoders:       Encoder[];
    Frontends:      Frontend[];
    JobQueue:       JobQueueJob[];
    MachineInfo:    MachineInfo;
    Miscellaneous:  string;
    ProtoVer:       string;
    Scheduled:      ScheduleOrProgram[];
    Version:        string;
}

export interface BackendStatusResponse {
    BackendStatus:  BackendStatus;
}