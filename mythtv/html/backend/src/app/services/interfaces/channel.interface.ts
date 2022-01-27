import { ScheduleOrProgram } from "./program.interface";

export interface Channel {
    ATSCMajorChan:      number;
    ATSCMinorChan:      number;
    CallSign:           string;
    ChanFilters:        string;
    ChanId:             number;
    ChanNum:            string; // number sent as string
    ChannelGroups:      string; // null in sample data
    ChannelName:        string;
    CommFree:           boolean;
    DefaultAuth:        string;
    ExtendedVisible:    string;
    FineTune:           number;
    Format:             string;
    FrequencyId:        string; // null in sample data
    IconURL:            string;
    InputId:            number;
    Inputs:             string; // null in sample data
    MplexId:            number;
    Programs:           ScheduleOrProgram[];
    ServiceId:          number;
    ServiceType:        number;
    SourceId:           number;
    UseEIT:             boolean;
    Visible:            boolean;
    XMLTVID:            string;
}