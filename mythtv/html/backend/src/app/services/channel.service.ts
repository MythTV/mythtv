import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import {
  Channel,
  ChannelRestoreData,
  ChannelScanRequest,
  ChannelScanStatus,
  CommMethodList,
  DBChannelRequest,
  FetchChannelsFromSourceRequest,
  GetChannelInfoListRequest,
  GetVideoMultiplexListRequest,
  Scan,
  ScanDialogResponse,
  UpdateVideoSourceRequest
} from './interfaces/channel.interface';
import { ChannelInfoList } from './interfaces/channelinfolist.interface';
import { BoolResponse, StringListResponse, StringResponse } from './interfaces/common.interface';
import { GetDDLineupListRequest, LineupList } from './interfaces/lineup.interface';
import { VideoMultiplex, VideoMultiplexList } from './interfaces/multiplex.interface';
import { FreqTableList, GrabberList, VideoSource, VideoSourceList } from './interfaces/videosource.interface';

@Injectable({
  providedIn: 'root'
})
export class ChannelService {

  constructor(private httpClient: HttpClient) { }

  public AddDBChannel(request: DBChannelRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/AddDBChannel', request);
  }

  public UpdateDBChannel(request: DBChannelRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/UpdateDBChannel', request);
  }

  public AddVideoSource(videosource: VideoSource): Observable<number> {
    return this.httpClient.post<number>('./Channel/AddVideoSource', videosource);
  }

  public FetchChannelsFromSource(request: FetchChannelsFromSourceRequest): Observable<number> {
    let params = new HttpParams()
      .set("SourceId", request.SourceId)
      .set("CardId", request.CardId)
      .set("WaitForFinish", request.WaitForFinish);
    return this.httpClient.get<number>('./Channel/FetchChannelsFromSource', { params });
  }

  public GetChannelInfo(channel: number): Observable<Channel> {
    let params = new HttpParams()
      .set("ChanID", channel);
    return this.httpClient.get<Channel>('./Channel/GetChannelInfo', { params });
  }

  public GetChannelInfoList(request: GetChannelInfoListRequest): Observable<ChannelInfoList> {
    let params = new HttpParams()
    if (request.SourceID !== undefined)
      params = params.set("SourceID", request.SourceID);
    if (request.ChannelGroupID !== undefined)
      params = params.set("ChannelGroupID", request.ChannelGroupID);
    if (request.StartIndex !== undefined)
      params = params.set("StartIndex", request.StartIndex);
    if (request.Count !== undefined)
      params = params.set("Count", request.Count);
    if (request.OnlyVisible !== undefined)
      params = params.set("OnlyVisible", request.OnlyVisible);
    if (request.Details !== undefined)
      params = params.set("Details", request.Details);
    if (request.OrderByName !== undefined)
      params = params.set("OrderByName", request.OrderByName);
    if (request.GroupByCallsign !== undefined)
      params = params.set("GroupByCallsign", request.GroupByCallsign);
    if (request.OnlyTunable !== undefined)
      params = params.set("OnlyTunable", request.OnlyTunable);
    return this.httpClient.get<ChannelInfoList>('./Channel/GetChannelInfoList', { params });
  }

  public GetDDLineupList(request: GetDDLineupListRequest): Observable<LineupList> {
    let params = new HttpParams()
      .set("Source", request.Source)
      .set("UserId", request.UserId)
      .set("Password", request.Password);
    return this.httpClient.get<LineupList>('./Channel/GetDDLineupList', { params });
  }

  public GetVideoMultiplex(mplexid: number): Observable<VideoMultiplex> {
    let params = new HttpParams()
      .set("MplexID", mplexid);
    return this.httpClient.get<VideoMultiplex>('./Channel/GetVideoMultiplex', { params });
  }

  public GetVideoMultiplexList(request: GetVideoMultiplexListRequest): Observable<VideoMultiplexList> {
    let params = new HttpParams()
      .set("SourceID", request.SourceID);
    if (request.StartIndex)
      params = params.set("StartIndex", request.StartIndex)
    if (request.Count)
      params = params.set("Count", request.Count);
    return this.httpClient.get<VideoMultiplexList>('./Channel/GetVideoMultiplexList', { params });
  }

  public GetVideoSource(sourceid: number): Observable<VideoSource> {
    let params = new HttpParams()
      .set("SourceID", sourceid);
    return this.httpClient.get<VideoSource>('./Channel/GetVideoSource', { params });
  }

  public GetVideoSourceList(): Observable<VideoSourceList> {
    return this.httpClient.get<VideoSourceList>('./Channel/GetVideoSourceList');
  }

  public GetXMLTVIdList(sourceid: number): Observable<StringListResponse> {
    let params = new HttpParams()
      .set("SourceID", sourceid);
    return this.httpClient.get<StringListResponse>('./Channel/GetXMLTVIdList', { params });
  }

  public GetAvailableChanid(): Observable<{ int: number }> {
    return this.httpClient.get<{ int: number }>('./Channel/GetAvailableChanid', {});
  }

  public RemoveDBChannel(channelid: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/RemoveDBChannel', { ChannelID: channelid });
  }

  public RemoveVideoSource(sourceid: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/RemoveVideoSource', { SourceId: sourceid });
  }

  public UpdateVideoSource(request: UpdateVideoSourceRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/UpdateVideoSource', request);
  }

  public GetGrabberList(): Observable<GrabberList> {
    return this.httpClient.get<GrabberList>('./Channel/GetGrabberList');
  }

  public GetFreqTableList(): Observable<FreqTableList> {
    return this.httpClient.get<FreqTableList>('./Channel/GetFreqTableList');
  }

  public GetCommMethodList(): Observable<CommMethodList> {
    return this.httpClient.get<CommMethodList>('./Channel/GetCommMethodList');
  }

  public StartScan(request: ChannelScanRequest): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/StartScan', request);
  }

  public GetScanStatus(): Observable<{ ScanStatus: ChannelScanStatus }> {
    return this.httpClient.get<{ ScanStatus: ChannelScanStatus }>('./Channel/GetScanStatus');
  }

  public StopScan(CardId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/StopScan', { Cardid: CardId });
  }

  public GetScanList(sourceid: number): Observable<{ ScanList: { Scans: Scan[] } }> {
    let params = new HttpParams()
      .set("SourceID", sourceid);
    return this.httpClient.get<{ ScanList: { Scans: Scan[] } }>('./Channel/GetScanList', { params });
  }

  public SendScanDialogResponse(request: ScanDialogResponse): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/SendScanDialogResponse', request);
  }

  public GetRestoreData (sourceId: number, xmltvId: boolean, icon: boolean, visible: boolean) : Observable<{ChannelRestore:ChannelRestoreData}> {
    let params = new HttpParams()
      .set("SourceId", sourceId)
      .set("XmltvId", xmltvId)
      .set("Icon", icon)
      .set("Visible", visible)
    return this.httpClient.get<{ChannelRestore:ChannelRestoreData}>('./Channel/GetRestoreData', { params });
  }

  public SaveRestoreData (sourceId: number) : Observable<BoolResponse> {
   return this.httpClient.post<BoolResponse>('./Channel/SaveRestoreData', { SourceId: sourceId });
  }

  public CopyIconToBackend(chanid: number, url: string) :  Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Channel/CopyIconToBackend', { ChanId: chanid, Url: url });
  }

}
