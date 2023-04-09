export interface RecProfParam {
    Name:             string;
    Value:            string|boolean;
}

export interface RecProfile {
    Id:               number;
    Name:             string;
    VideoCodec:       string;
    AudioCodec:       string;
    RecProfParams:    RecProfParam[];
}

export interface RecProfileGroup {
    Id:               number;
    Name:             string;
    CardType:         string;
    RecProfiles:      RecProfile[];
}

export interface RecProfileGroupList {
    RecProfileGroupList : {
        RecProfileGroups: RecProfileGroup [];
    }
}
