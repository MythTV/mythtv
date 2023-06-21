// Note - the name "Input" conflicts with the Angular
// Input class, so you cannot declare an Input object
// aywhere other than in this file, and expect it to work.

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
    Inputs:         Input[] ;
}
