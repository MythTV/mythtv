import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { GetVideoListRequest, UpdateVideoMetadataRequest, VideoMetadataInfoList, VideoCategoryList } from './interfaces/video.interface';
import { Observable } from 'rxjs';
import { BoolResponse } from './interfaces/common.interface';

@Injectable({
  providedIn: 'root'
})
export class VideoService {

  constructor(private httpClient: HttpClient) { }

  // All parameters are optional
  public GetVideoList(request: GetVideoListRequest):
    Observable<{ VideoMetadataInfoList: VideoMetadataInfoList }> {
    let params = new HttpParams();
    for (const [key, value] of Object.entries(request))
      params = params.set(key, value);
    return this.httpClient.get<{ VideoMetadataInfoList: VideoMetadataInfoList }>
      ('./Video/GetVideoList', { params });
  }

  public GetCategoryList(): Observable<VideoCategoryList> {
    return this.httpClient.get<VideoCategoryList>('./Video/GetCategoryList');
  }

  public UpdateVideoWatchedStatus(id: number, watched: boolean) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Video/UpdateVideoWatchedStatus', {Id: id, Watched: watched});
  }

  public UpdateVideoMetadata(request: UpdateVideoMetadataRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Video/UpdateVideoMetadata', request);
  }

}
