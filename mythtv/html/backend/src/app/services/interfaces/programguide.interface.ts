import { Channel } from "./channel.interface";

export interface ProgramGuide {
    StartTime:      string; // DateTime
    EndTime:        string; // DateTime
    Details:        boolean;
    StartIndex:     number;
    Count:          number;
    TotalAvailable: number;
    AsOf:           string; // DateTime
    Version:        string;
    ProtoVer:       string;
    Channels:       Channel[];
}