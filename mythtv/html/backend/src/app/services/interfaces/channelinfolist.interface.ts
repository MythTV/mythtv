import { Channel } from "./channel.interface";
export interface ChannelInfoList {
    ChannelInfoList: {
        StartIndex: number;
        Count: number;
        CurrentPage: number;
        TotalPages: number;
        TotalAvailable: number;
        AsOf: string; // DateTime
        Version: string;
        ProtoVer: string;
        ChannelInfos: Channel[];
    }
}