import { Channel } from "./channel.interface";

export interface ProgramGuide {
    ProgramGuide: {
        AsOf:           string; // DateTime
        Channels:       Channel[];
        Count:          number;
        Details:        boolean;
        EndTime:        string; // DateTime
        ProtoVer:       string;
        StartIndex:     number;
        StartTime:      string; // DateTime
        TotalAvailable: number;
        Version:        string;
    }
}