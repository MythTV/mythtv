import { LIVE_ANNOUNCER_DEFAULT_OPTIONS } from "@angular/cdk/a11y";

export interface GetDDLineupListRequest {
    Source:             string;
    UserId:             string;
    Password:           string;
}

export interface Lineup {
    LineupId:           string;
    Name:               string;
    DisplayName:        string;
    Type:               string;
    Postal:             string;
    Device:             string;
}

export interface LineupList {
    Lineups:            Lineup[];
}
