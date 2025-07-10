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
    InputName:               string;
}

export interface CardInput {
    CardId:                number;
    SourceId:              number;
    ExternalCommand:       string;
    ChangerDevice:         string;
    ChangerModel:          string;
    HostName:              string;
    TuneChannel:           string;
    StartChannel:          string;
    DisplayName:           string;
    DishnetEIT:            boolean;
    RecPriority:           number;
    Quicktune:             number;
    SchedOrder:            number;
    LiveTVOrder:           number;
    RecLimit:              number;
    SchedGroup:            boolean;
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
    VideoDevicePrompt: string;
    AudioDevices:     string [];
    FrontendName:     string;
    InputNames:       string [];
    DefaultInputName: string;
    Description:      string;
    FirewireModel:    string;
    IPAddress:        string;
    TunerType:        string;
    TunerNumber:      number;
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
    DiSEqCId:            number;
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

export interface DiseqcConfig {
    CardId:            number;
    DiSEqCId:          number;
    Value:             string;
}

export interface DiseqcConfigList {
    DiseqcConfigList : {
        DiseqcConfigs: DiseqcConfig [];
    }
}


export interface DiseqcParm {
    description: string,
    type: string,
    inactive: boolean
}

export interface InputGroup {
    CardInputId: number;
    InputGroupId: number;
    InputGroupName: string;
}

export interface InputGroupList {
    InputGroupList: {
        InputGroups: InputGroup[];
    }
}

export interface CardSubType {
    // CardSubType : {
        CardId:         number;
        SubType:        string;
        InputType:      string;
        HDHRdoesDVBC:   boolean;
        HDHRdoesDVB:    boolean;
    // }
}
