import { ProgramList } from "./program.interface";

export interface GetRecStorageGroupListResponse {
    RecStorageGroupList:    String[];
}

export interface GetUpcomingListResponse {
    ProgramList:            ProgramList;
}
