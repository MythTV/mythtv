import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import {
  AddDBChannelRequest,
  Channel,
  FetchChannelsFromSourceRequest,
  GetChannelInfoListRequest,
  GetVideoMultiplexListRequest,
  UpdateVideoSourceRequest } from './interfaces/channel.interface';
import { ChannelInfoList } from './interfaces/channelinfolist.interface';
import { BoolResponse } from './interfaces/common.interface';
import { GetDDLineupListRequest, LineupList } from './interfaces/lineup.interface';
import { VideoMultiplex, VideoMultiplexList } from './interfaces/multiplex.interface';
import { VideoSource, VideoSourceList } from './interfaces/videosource.interface';

@Injectable({
  providedIn: 'root'
})
export class ChannelService {

  constructor(private httpClient: HttpClient) { }

  public AddDBChannel(request : AddDBChannelRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Channel/AddDBChannel', request);
  }

  public AddVideoSource(videosource : VideoSource) : Observable<number> {
    return this.httpClient.post<number>('/Channel/AddVideoSource', videosource);
  }

  public FetchChannelsFromSource(request : FetchChannelsFromSourceRequest) : Observable<number> {
    let params = new HttpParams()
      .set("SourceId", request.SourceId)
      .set("CardId", request.CardId)
      .set("WaitForFinish", request.WaitForFinish);
    return this.httpClient.get<number>('/Channel/FetchChannelsFromSource', {params});
  }

  public GetChannelInfo(channel : number) : Observable<Channel> {
    let params = new HttpParams()
      .set("ChanID", channel);
    return this.httpClient.get<Channel>('/Channel/GetChannelInfo', {params});
  }

  public GetChannelInfoList(request : GetChannelInfoListRequest) : Observable<ChannelInfoList> {
    let params = new HttpParams()
      .set("SourceID", request.SourceID)
      .set("ChannelGroupID", request.ChannelGroupID)
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count)
      .set("OnlyVisible", request.OnlyVisible)
      .set("Details", request.Details)
      .set("OrderByName", request.OrderByName)
      .set("GroupByCallsign", request.GroupByCallsign)
      .set("OnlyTunable", request.OnlyTunable);
    return this.httpClient.get<ChannelInfoList>('/Channel/GetChannelInfoList', {params});
  }

  public GetDDLineupList(request : GetDDLineupListRequest) : Observable<LineupList> {
    let params = new HttpParams()
      .set("Source", request.Source)
      .set("UserId", request.UserId)
      .set("Password", request.Password);
    return this.httpClient.get<LineupList>('/Channel/GetDDLineupList', {params});
  }

  public GetVideoMultiplex(mplexid : number) : Observable<VideoMultiplex> {
    let params = new HttpParams()
      .set("MplexID", mplexid);
    return this.httpClient.get<VideoMultiplex>('/Channel/GetVideoMultiplex', {params});
  }

  public GetVideoMultiplexList(request: GetVideoMultiplexListRequest) : Observable<VideoMultiplexList> {
    let params = new HttpParams()
      .set("SourceID", request.SourceID)
      .set("StartIndex", request.StartIndex)
      .set("Count", request.Count);
    return this.httpClient.get<VideoMultiplexList>('/Channel/GetVideoMultiplexLost', {params});
  }

  public GetVideoSource(sourceid : number) : Observable<VideoSource> {
    let params = new HttpParams()
      .set("SourceID", sourceid);
    return this.httpClient.get<VideoSource>('/Channel/GetVideoSource', {params});
  }

  public GetVideoSourceList() : Observable<VideoSourceList> {
    return this.httpClient.get<VideoSourceList>('/Channel/GetVideoSourceList');
  }

  public GetXMLTVIdList(sourceid : number) : Observable<String[]> {
    let params = new HttpParams()
      .set("SourceID", sourceid);
    return this.httpClient.get<String[]>('/Channel/GetXMLTVidList', {params});
  }

  public RemoveDBChannel(channelid : number) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Channel/RemoveDBChannel', channelid);
  }

  public RemoveVideoSource(sourceid : number) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Channel/RemoveVideoSource', sourceid);
  }

  public UpdateVideoSource(request : UpdateVideoSourceRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Channel/UpdateVideoSource', request);
  }
}
