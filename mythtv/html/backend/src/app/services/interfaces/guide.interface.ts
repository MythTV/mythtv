export interface ChannelGroupRequest {
    ChannelGroupId:     number;
    ChanId:             number;
}

export interface GetCategoryListResponse {
    CategoryList:       string[];
}

export interface GetChannelIconRequest {
    ChanId:             number;
    Width:              number;
    Height:             number;
}

export interface GetProgramDetailsRequest {
    ChanId:             number;
    StartTime:          string; // dateTime
}

export interface GetProgramGuideRequest {
    StartTime:          string;
    EndTime:            string;
    Details?:           boolean; // really a boolean
    ChannelGroupId?:    number;
    StartIndex?:        number;
    Count?:             number;
    WithInvisible?:     boolean;
}

export interface GetProgramListRequest {
    StartIndex?:        number;
    Count?:             number;
    StartTime?:         string; // dateTime
    EndTime?:           string; // dateTime
    ChanId?:            number;
    TitleFilter?:       string;
    CategoryFilter?:    string;
    PersonFilter?:      string;
    KeywordFilter?:     string;
    OnlyNew?:           boolean;
    Details?:           boolean;
    Sort?:              string;
    Descending?:        boolean;
    WithInvisible?:     boolean;
    CatType?:           string;
}

export interface GetStoredSearchesResponse {
    StoredSearches:     String[];
}
