export interface Input {
    Id:             number;
    CardId:         number;
    SourceId:       number;
    InputName:      string;
    DisplayName:    string;
    QuickTune:      boolean;
    RecPriority:    number;
    ScheduleOrder:  number;
    LiveTVOrder:    number;
}

export interface InputList {
    Inputs:         Input[];
}
