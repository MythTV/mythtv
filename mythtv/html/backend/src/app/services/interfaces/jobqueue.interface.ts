import { ScheduleOrProgram } from "./program.interface";

export interface JobQueueJob {
    Args:           string;
    ChanId:         number;
    Cmds:           number;
    Comment:        string;
    Flags:          number;
    HostName:       string;
    Id:             number;
    InsertTime:     string;
    Program:        ScheduleOrProgram;
    SchedRunTime:   string;
    StartTime:      string;
    StartTs:        string;
    Status:         number;
    StatusTime:     string;
    Type:           number;
    LocalizedStatus:  string;
    LocalizedJobName: string;
}
