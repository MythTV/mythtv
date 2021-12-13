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
    Recording:      number;
    SleepStatus:    number;
    State:          number;
}
