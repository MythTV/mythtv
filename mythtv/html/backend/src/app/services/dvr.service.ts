import { Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
import {
  AddDontRecordScheduleRequest,
  AddRecordedCreditsRequest,
  AddRecordScheduleRequest,
  DeleteRecordingRequest,
  GetConflictListRequest,
  GetExpiringListRequest,
  GetLastPlayPosRequest,
  GetOldRecordedListRequest,
  GetPlayGroupListResponse,
  GetProgramCategoriesResponse,
  GetRecGroupListResponse,
  GetRecordedRequest,
  GetRecordScheduleListRequest,
  GetRecordScheduleRequest,
  GetRecStorageGroupListResponse,
  GetUpcomingListResponse
} from './interfaces/dvr.interface';
import { BoolResponse } from './interfaces/common.interface';
import { ProgramList, ScheduleOrProgram } from './interfaces/program.interface';
import { EncoderList } from './interfaces/encoder.interface';
import { InputList } from './interfaces/input.interface';
import { RecRule, RecRuleFilterList, RecRuleList } from './interfaces/recording.interface';

@Injectable({
  providedIn: 'root'
})
export class DvrService {

  constructor(private httpClient: HttpClient) { }

  public AddDontRecordSchedule(request : AddDontRecordScheduleRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/AddDontRecordSchedule', request);
  }

  public AddRecordSchedule(request : AddRecordScheduleRequest) : Observable<number> {
    return this.httpClient.post<number>('/Dvr/AddRecordSchedule', request);
  }

  public AddRecordedCredits(request : AddRecordedCreditsRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/AddRecordedCredits', request);
  }

  public AddRecordedProgram(json : string) : Observable<number> {
    return this.httpClient.post<number>('/Dvr/AddRecordedProgram', json);
  }

  public AllowReRecord(RecordedId : number) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/AllowReRecord', RecordedId);
  }

  public DeleteRecording(request : DeleteRecordingRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/DeleteRecording', request);
  }

  public DisableRecordSchedule(recordid : number) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/DisableRecordSchedule', recordid);
  }

  public DupInToDescription(DupIn : string) : Observable<string> {
    let params = new HttpParams()
      .set("DupIn", DupIn);
    return this.httpClient.get<string>('/Dvr/DupInToDescription', {params});
  }

  public DupInToString(DupIn : string) : Observable<string> {
    let params = new HttpParams()
      .set("DupIn", DupIn);
    return this.httpClient.get<string>('/Dvr/DupInToString', {params});
  }

  public DupMethodToDescription(DupMethod : string) : Observable<string> {
    let params = new HttpParams()
      .set("DupMethod", DupMethod);
    return this.httpClient.get<string>('/Dvr/DupMethodToDescription', {params});
  }

  public DupMethodToString(DupMethod : string) : Observable<string> {
    let params = new HttpParams()
      .set("DupMethod", DupMethod);
    return this.httpClient.get<string>('/Dvr/DupMethodToString', {params});
  }

  public EnableRecordSchedule(recordid : number) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/EnableRecordSchedule', recordid);
  }

  public GetConflictList(request : GetConflictListRequest) : Observable<ProgramList> {
    let params = new HttpParams()
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count)
      .set("RecordId", request.RecordId)
    return this.httpClient.get<ProgramList>('/Dvr/GetConflictList', {params});
  }

  public GetEncoderList() : Observable<EncoderList> {
    return this.httpClient.get<EncoderList>('/Dvr/GetEncoderList');
  }

  public GetExpiringList(request : GetExpiringListRequest) : Observable<ProgramList> {
    let params = new HttpParams()
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count);
    return this.httpClient.get<ProgramList>('/Dvr/GetExpiringList', {params});
  }

  public GetInputList() : Observable<InputList> {
    return this.httpClient.get<InputList>('/Dvr/GetInputList');
  }

  public GetLastPlayPos(request : GetLastPlayPosRequest) : Observable<number> {
    let params = new HttpParams()
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime)
      .set("OffsetType", request.OffsetType);
    return this.httpClient.get<number>('/Dvr/GetLastPlayPos', {params});
  }

  public GetOldRecordedList(request : GetOldRecordedListRequest) : Observable<ProgramList> {
    let params = new HttpParams()
      .set("Descending", request.Descending)
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count)
      .set("StartTime", request.StartTime)
      .set("EndTime", request.EndTime)
      .set("Title", request.Title)
      .set("SeriesId", request.SeriesId)
      .set("RecordId", request.RecordId)
      .set("Sort", request.Sort);
    return this.httpClient.get<ProgramList>('/Dvr/GetOldRecordedList', {params});
  }

  public GetPlayGroupList() : Observable<GetPlayGroupListResponse> {
    return this.httpClient.get<GetPlayGroupListResponse>('/Dvr/GetPlayGroupList');
  }

  public GetProgramCategories(OnlyRecorded : boolean) : Observable<GetProgramCategoriesResponse> {
    let params = new HttpParams()
      .set("OnlyRecorded", OnlyRecorded);
    return this.httpClient.get<GetProgramCategoriesResponse>('/Dvr/GetProgramCategories')
  }

  public GetRecGroupList() : Observable<GetRecGroupListResponse> {
    return this.httpClient.get<GetRecGroupListResponse>('/Dvr/GetRecGroupList');
  }

  public GetRecRuleFilterList() : Observable<RecRuleFilterList> {
    return this.httpClient.get<RecRuleFilterList>('/Dvr/GetRecRuleFilterList');
  }

  public GetRecStorageGroupList() : Observable<GetRecStorageGroupListResponse> {
    return this.httpClient.get<GetRecStorageGroupListResponse>('/Dvr/GetRecStorageGroupList');
  }

  public GetRecordSchedule(request : GetRecordScheduleRequest) : Observable<RecRule> {
    let params = new HttpParams()
      .set("RecordId", request.RecordId)
      .set("Template", request.Template)
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime)
      .set("MakeOverride", request.MakeOverride);
    return this.httpClient.get<RecRule>('/Dvr/GetRecordSchedule', {params});
  }

  public GetRecordScheduleList(request : GetRecordScheduleListRequest) : Observable<RecRuleList> {
    let params = new HttpParams()
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count)
      .set("Sort", request.Sort)
      .set("Descending", request.Descending);
    return this.httpClient.get<RecRuleList>('/Dvr/GetRecordScheduleList', {params});
  }

  public GetRecorded(request : GetRecordedRequest) : Observable<ScheduleOrProgram> {
    let params = new HttpParams()
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime);
    return this.httpClient.get<ScheduleOrProgram>('/Dvr/GetRecorded', {params});
  }
  // TODO: from here

  public GetUpcomingList() : Observable<GetUpcomingListResponse> {
    return this.httpClient.get<GetUpcomingListResponse>('/Dvr/GetUpcomingList');
  }
}
