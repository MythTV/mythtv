export interface VideoMultiplex {
    MplexId:            number;
    SourceId:           number;
    TransportId:        number;
    NetworkId:          number;
    Frequency:          number;
    Inversion:          string;
    SymbolRate:         number;
    FEC:                string;
    Polarity:           string;
    Modulation:         string;
    Bandwidth:          string;
    LPCodeRate:         string;
    HPCodeRate:         string;
    TransmissionMode:   string;
    GuardInterval:      string;
    Visible:            boolean;
    Constellation:      string;
    Hierarchy:          string;
    ModulationSystems:  string;
    RollOff:            string;
    SIStandard:         string;
    ServiceVersion:     number;
    UpdateTimeStamp:    string; // dateTime
    DefaultAuthority:   string;
    Description:        string;
}

export interface VideoMultiplexList {
    VideoMultiplexList : {
        StartIndex:         number;
        Count:              number;
        CurrentPage:        number;
        TotalPages:         number;
        TotalAvailable:     number;
        AsOf:               string; // dateTime
        Version:            string;
        ProtoVer:           string;
        VideoMultiplexes:   VideoMultiplex[];
    }
}
