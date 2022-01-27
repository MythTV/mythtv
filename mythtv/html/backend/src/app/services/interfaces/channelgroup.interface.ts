export interface ChannelGroup {
    GroupId:        number;
    Name:           string;
    Password:       string;
}

export interface ChannelGroupList {
    ChannelGroups:  ChannelGroup[];
}