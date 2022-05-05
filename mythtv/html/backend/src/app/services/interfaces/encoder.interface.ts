import { ScheduleOrProgram } from "./program.interface";

// Definitions from libs/libmythtv/tv.h
export enum TVState {
    kState_Error = -1,
    kState_None,
    kState_WatchingLiveTV,
    kState_WatchingPreRecorded,
    kState_WatchingVideo,
    kState_WatchingDVD,
    kState_WatchingBD,
    kState_WatchingRecording,
    kState_RecordingOnly,
    kState_ChangingState,
}
export interface EncoderInput {
    CardId:         number;
    DisplayName:    string;
    Id:             number;
    InputName:      string;
    LiveTVOrder:    number;
    QuickTune:      boolean;
    RecPriority:    number;
    ScheduleOrder:  number;
    SourceId:       number;
}

export interface Encoder {
    Connected:      boolean;
    HostName:       string;
    Id:             number;
    Inputs:         EncoderInput[];
    Local:          boolean;
    LowOnFreeSpace: boolean;
    Recording:      ScheduleOrProgram;
    SleepStatus:    number;
    State:          number;
}

export interface EncoderList {
    Encoders:       Encoder[];
}
