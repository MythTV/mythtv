import { Encoder } from "./encoder.interface";
import { JobQueueJob } from "./jobqueue.interface";
import { ScheduleOrProgram } from "./program.interface";
import { StorageGroup } from "./storagegroup.interface";
import { Backend } from "./backend.interface";
import { Frontend } from "./frontend.interface";

export interface MachineInfo {
    LoadAvg1:       number;
    LoadAvg2:       number;
    LoadAvg3:       number;
    GuideStart:     string; // date
    GuideEnd:       string; // date
    GuideThru:      string; // date
    GuideDays:      number;
    GuideNext:      string; // date
    GuideStatus:    string;
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