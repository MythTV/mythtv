export interface CaptureCard {
    VideoDevice:             string;
    AudioDevice:             string;
    VBIDevice:               string;
    CardType:                string;
    AudioRateLimit:          number;
    HostName:                string;
    DVBSWFilter:             number;
    DVBSatType:              number;
    DVBWaitForSeqStart:      boolean;
    SkipBTAudio:             boolean;
    DVBOnDemand:             boolean;
    DVBDiSEqCType:           number;
    FirewireSpeed:           number;
    FirewireModel:           string;
    FirewireConnection:      number;
    SignalTimeout:           number;
    ChannelTimeout:          number;
    DVBTuningDelay:          number;
    Contrast:                number;
    Brightness:              number;
    Colour:                  number;
    Hue:                     number;
    DiSEqCId:                number;
    DVBEITScan:              boolean;
}

export interface CardInput {
    CardId:                number;
    SourceId:              number;
    InputName:             string;
    ExternalCommand:       string;
    ChangerDevice:         string;
    ChangerModel:          string;
    HostName:              string;
    TuneChan:              string;
    StartChan:             string;
    DisplayName:           string;
    DishnetEIT:            boolean;
    RecPriority:           number;
    Quicktune:             number;
    SchedOrder:            number;
    LiveTVOrder:           number;
}

export interface CardAndInput extends CaptureCard, CardInput {
    ParentId:              number;
}

export interface CaptureCardList {
    CaptureCardList: {
        CaptureCards:  CardAndInput[];
    }
}

export interface CardType {
    CardType:       string;
    Description:    string;
}

export interface CardTypeList {
    CardTypeList : {
        CardTypes: CardType [];
    }
}

export interface CaptureDevice {
    CardType:       string;
    SubType:          string;
    VideoDevice:      string;
    FrontendName:     string;
    InputNames:       string [];
    DefaultInputName: string;
    SignalTimeout:    number;
    ChannelTimeout:   number;
    TuningDelay:      number;
}

export interface CaptureDeviceList {
    CaptureDeviceList : {
        CaptureDevices: CaptureDevice [];
    }
}

export interface DiseqcTree {
    DiseqcId:            number;
    ParentId:            number;
    Ordinal:             number;
    Type:                string;
    SubType:             string;
    Description:         string;
    SwitchPorts:         number;
    RotorHiSpeed:        number;
    RotorLoSpeed:        number;
    RotorPositions:      string;
    LnbLofSwitch:        number;
    LnbLofHi:            number;
    LnbLofLo:            number;
    CmdRepeat:           number;
    LnbPolInv:           boolean;
    Address:             number;
    ScrUserband:         number;
    ScrFrequency:        number;
    ScrPin:              number;
}

export interface DiseqcTreeList {
    DiseqcTreeList : {
        DiseqcTrees: DiseqcTree [];
    }
}
