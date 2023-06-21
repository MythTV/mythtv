import { Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
import {
  AddDontRecordScheduleRequest,
  AddRecordedCreditsRequest,
  DeleteRecordingRequest,
  GetConflictListRequest,
  GetExpiringListRequest,
  GetLastPlayPosRequest,
  GetOldRecordedListRequest,
  PlayGroupList,
  ProgramCategories,
  RecGroupList,
  GetRecordedListRequest,
  GetRecordedRequest,
  GetRecordScheduleListRequest,
  GetRecordScheduleRequest,
  RecStorageGroupList,
  UpcomingList,
  GetUpcomingRequest,
  UnDeleteRecordingRequest,
  UpdateRecordedMetadataRequest,
  RecordScheduleRequest
} from './interfaces/dvr.interface';
import { BoolResponse, StringResponse } from './interfaces/common.interface';
import { ProgramList, ScheduleOrProgram } from './interfaces/program.interface';
import { EncoderList } from './interfaces/encoder.interface';
import { InputList } from './interfaces/input.interface';
import { RecRule, RecRuleFilterList, RecRuleList } from './interfaces/recording.interface';

@Injectable({
  providedIn: 'root'
})
export class DvrService {

  constructor(private httpClient: HttpClient) { }

  public AddDontRecordSchedule(request: AddDontRecordScheduleRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/AddDontRecordSchedule', request);
  }

  public AddRecordSchedule(request: RecordScheduleRequest): Observable<number> {
    return this.httpClient.post<number>('/Dvr/AddRecordSchedule', request);
  }

  public UpdateRecordSchedule(request: RecordScheduleRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/UpdateRecordSchedule', request);
  }

  public RemoveRecordSchedule(RecordId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/RemoveRecordSchedule', {RecordId: RecordId});
  }

  public AddRecordedCredits(request: AddRecordedCreditsRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/AddRecordedCredits', request);
  }

  public AddRecordedProgram(json: string): Observable<number> {
    return this.httpClient.post<number>('/Dvr/AddRecordedProgram', json);
  }

  public AllowReRecord(RecordedId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/AllowReRecord', { RecordedId: RecordedId });
  }

  public DeleteRecording(request: DeleteRecordingRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/DeleteRecording', request);
  }

  public UnDeleteRecording(request: UnDeleteRecordingRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/UnDeleteRecording', request);
  }

  public UpdateRecordedMetadata(request: UpdateRecordedMetadataRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/UpdateRecordedMetadata', request);
  }

  public DisableRecordSchedule(recordid: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/DisableRecordSchedule', {RecordId: recordid});
  }

  public DupInToDescription(DupIn: string): Observable<string> {
    let params = new HttpParams()
      .set("DupIn", DupIn);
    return this.httpClient.get<string>('/Dvr/DupInToDescription', { params });
  }

  public DupInToString(DupIn: string): Observable<string> {
    let params = new HttpParams()
      .set("DupIn", DupIn);
    return this.httpClient.get<string>('/Dvr/DupInToString', { params });
  }

  public DupMethodToDescription(DupMethod: string): Observable<string> {
    let params = new HttpParams()
      .set("DupMethod", DupMethod);
    return this.httpClient.get<string>('/Dvr/DupMethodToDescription', { params });
  }

  public DupMethodToString(DupMethod: string): Observable<string> {
    let params = new HttpParams()
      .set("DupMethod", DupMethod);
    return this.httpClient.get<string>('/Dvr/DupMethodToString', { params });
  }

  public EnableRecordSchedule(recordid: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Dvr/EnableRecordSchedule', recordid);
  }

  public GetConflictList(request: GetConflictListRequest): Observable<ProgramList> {
    let params = new HttpParams()
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count)
      .set("RecordId", request.RecordId)
    return this.httpClient.get<ProgramList>('/Dvr/GetConflictList', { params });
  }

  public GetEncoderList(): Observable<EncoderList> {
    return this.httpClient.get<EncoderList>('/Dvr/GetEncoderList');
  }

  public GetExpiringList(request: GetExpiringListRequest): Observable<ProgramList> {
    let params = new HttpParams()
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count);
    return this.httpClient.get<ProgramList>('/Dvr/GetExpiringList', { params });
  }

  public GetInputList(): Observable<{InputList: InputList}> {
    return this.httpClient.get<{InputList: InputList}>('/Dvr/GetInputList');
  }

  public GetLastPlayPos(request: GetLastPlayPosRequest): Observable<number> {
    let params = new HttpParams()
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime)
      .set("OffsetType", request.OffsetType);
    return this.httpClient.get<number>('/Dvr/GetLastPlayPos', { params });
  }

  public GetOldRecordedList(request: GetOldRecordedListRequest): Observable<ProgramList> {
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
    return this.httpClient.get<ProgramList>('/Dvr/GetOldRecordedList', { params });
  }

  public GetPlayGroupList(): Observable<PlayGroupList> {
    return this.httpClient.get<PlayGroupList>('/Dvr/GetPlayGroupList');
  }

  public GetProgramCategories(OnlyRecorded: boolean): Observable<ProgramCategories> {
    let params = new HttpParams()
      .set("OnlyRecorded", OnlyRecorded);
    return this.httpClient.get<ProgramCategories>('/Dvr/GetProgramCategories')
  }

  public GetRecGroupList(): Observable<RecGroupList> {
    return this.httpClient.get<RecGroupList>('/Dvr/GetRecGroupList');
  }

  public GetRecRuleFilterList(): Observable<{RecRuleFilterList: RecRuleFilterList}> {
    return this.httpClient.get<{RecRuleFilterList: RecRuleFilterList}>('/Dvr/GetRecRuleFilterList');
  }

  public GetRecStorageGroupList(): Observable<RecStorageGroupList> {
    return this.httpClient.get<RecStorageGroupList>('/Dvr/GetRecStorageGroupList');
  }

  public GetRecordSchedule(request: GetRecordScheduleRequest): Observable<{RecRule: RecRule}> {
    let params = new HttpParams()
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<{RecRule: RecRule}>('/Dvr/GetRecordSchedule', { params });
  }

  public GetRecordScheduleList(request: GetRecordScheduleListRequest): Observable<{RecRuleList: RecRuleList}> {
    let params = new HttpParams()
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<{RecRuleList: RecRuleList}>('/Dvr/GetRecordScheduleList', { params });
  }

  public GetRecorded(request: GetRecordedRequest): Observable<{ Program: ScheduleOrProgram }> {
    let params = new HttpParams()
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<{ Program: ScheduleOrProgram }>('/Dvr/GetRecorded', { params });
  }

  // All parameters are optional
  public GetRecordedList(request: GetRecordedListRequest): Observable<{ ProgramList: ProgramList }> {
    let params = new HttpParams();
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<{ ProgramList: ProgramList }>('/Dvr/GetRecordedList', { params });
  }

  // All parameters are optional
  public GetUpcomingList(request: GetUpcomingRequest): Observable<UpcomingList> {
    let params = new HttpParams();
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<UpcomingList>('/Dvr/GetUpcomingList', { params });
  }

  public RecStatusToString(recStatus: number): Observable<StringResponse> {
    let params = new HttpParams()
      .set("RecStatus", recStatus);
    return this.httpClient.get<StringResponse>('/Dvr/RecStatusToString', { params });
  }
}
