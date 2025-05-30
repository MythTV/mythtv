import { Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';

import { ProgramGuide } from './interfaces/programguide.interface';
import {
  ChannelGroupRequest,
  GetCategoryListResponse,
  GetChannelIconRequest,
  GetProgramDetailsRequest,
  GetProgramGuideRequest,
  GetProgramListRequest,
  GetStoredSearchesResponse
} from './interfaces/guide.interface';
import { BoolResponse } from './interfaces/common.interface';
import { ChannelGroupList } from './interfaces/channelgroup.interface';
import { ProgramList, ScheduleOrProgram } from './interfaces/program.interface';

@Injectable({
  providedIn: 'root'
})
export class GuideService {
  startDate: Date;
  guide_data$!: ProgramGuide;
  guide_hours = 3;
  guide_millisecs = this.guide_hours * 60 * 60 * 1000;

  toTime(date: string) {
    let d = new Date(date);
    return d.toISOString();
  }
  toStartTime(date: string) {
    return this.toTime(date);
  }
  toEndTime(date: string) {
    let d = new Date(date);
    //let tomorrow = new Date(d.getTime()+86400000);
    let endAt = new Date(d.getTime() + this.guide_millisecs);
    return this.toTime(endAt.toISOString());
  }
  toHalfHour(date: Date) {
    let d = new Date(date);
    d.setMinutes((d.getMinutes() < 30) ? 0 : 30);
    d.setSeconds(0);
    return d;
  }

  constructor(private httpClient: HttpClient) {
    this.startDate = new Date;
  }

  public AddToChannelGroup(request: ChannelGroupRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Guide/AddToChannelGroup', request);
  }

  public GetCategoryList(): Observable<GetCategoryListResponse> {
    return this.httpClient.get<GetCategoryListResponse>('./Guide/GetCategoryList');
  }

  public GetChannelGroupList(IncludeEmpty: boolean): Observable<ChannelGroupList> {
    let params = new HttpParams()
      .set("IncludeEmpty", IncludeEmpty);
    return this.httpClient.get<ChannelGroupList>('./Guide/GetChannelGroupList', { params });
  }

  public GetChannelIcon(request: GetChannelIconRequest): Observable<string> {
    let params = new HttpParams()
      .set("ChanId", request.ChanId)
      .set("Width", request.Width)
      .set("Height", request.Height)
    return this.httpClient.get<string>('./Guide/GetChannelIcon', { params });
  }

  public GetProgramDetails(request: GetProgramDetailsRequest): Observable<{Program: ScheduleOrProgram}> {
    let params = new HttpParams()
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime);
    return this.httpClient.get<{Program: ScheduleOrProgram}>('./Guide/GetProgramDetails', { params });
  }

  public GetProgramGuide(reqDate?: Date, ChannelGroupId?: number): Observable<ProgramGuide> {
    if (reqDate) {
      this.startDate = reqDate;
    }
    let time: string = this.toHalfHour(this.startDate).toISOString();
    let params: GetProgramGuideRequest = {
      "StartTime": this.toStartTime(time),
      "EndTime": this.toEndTime(time),
      "Details": true,
      "ChannelGroupId" : ChannelGroupId
    };
    return this.httpClient.post<ProgramGuide>('./Guide/GetProgramGuide', params);
  }

  public GetProgramList(request: GetProgramListRequest): Observable<{ ProgramList: ProgramList }> {
    let params = new HttpParams()
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<{ ProgramList: ProgramList }>('./Guide/GetProgramList', { params });
  }

  public GetStoredSearches(searchType: string): Observable<GetStoredSearchesResponse> {
    let params = new HttpParams()
      .set("Type", searchType);
    return this.httpClient.get<GetStoredSearchesResponse>('./Guide/GetStoredSearches', { params });
  }

  public RemoveFromChannelGroup(request: ChannelGroupRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Guide/RemoveFromChannelGroup', request);
  }

  public AddChannelGroup(Name: string): Observable<number> {
    return this.httpClient.post<number>('./Guide/AddChannelGroup', {Name: Name});
  }

  public UpdateChannelGroup(oldName: string, newName: string): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Guide/UpdateChannelGroup', {OldName: oldName, newName: newName});
  }

  public RemoveChannelGroup(Name: string): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Guide/RemoveChannelGroup', {Name: Name});
  }

}
