export interface GetProgramGuideRequest {
    StartTime:          string;
    EndTime:            string;
    Details:            string; // really a boolean
    ChannelGroupId?:    number;
    StartIndex?:        number;
    Count?:             number;
    WithInvisible?:     boolean;
}
