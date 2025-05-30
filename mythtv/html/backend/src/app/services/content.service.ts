import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { ArtworkInfoList } from './interfaces/artwork.interface';
import { BoolResponse } from './interfaces/common.interface';
import {
  DownloadFileRequest,
  GetAlbumArtRequest,
  StorageGroupRequest,
  StorageGroupFileNameRequest,
  GetImageFileRequest,
  GetPreviewImageRequest,
  GetProgramArtworkListRequest,
  GetRecordingRequest,
  GetRecordingArtworkRequest,
  GetRecordingArtworkListRequest,
  GetVideoArtworkRequest,
  GetDirListResponse,
  GetFileListResponse,
  GetHashResponse
} from './interfaces/content.interface';

@Injectable({
  providedIn: 'root'
})
export class ContentService {

  constructor(private httpClient: HttpClient) { }

  public DownloadFile(request : DownloadFileRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Content/DownloadFile', request);
  }

  public GetAlbumArt(request : GetAlbumArtRequest) : Observable<string> {
    let params = new HttpParams()
      .set("Id", request.Id)
      .set("Width", request.Width)
      .set("Height", request.Height);
    return this.httpClient.get<string>('./Content/GetAlbumArt', {params});
  }

  // Gets list of directories in the SG
  public GetDirList(request : StorageGroupRequest) : Observable<GetDirListResponse> {
    let params = new HttpParams()
      .set("StorageGroup", request.StorageGroup);
    return this.httpClient.get<GetDirListResponse>('./Content/GetDirList', {params});
  }

  public GetFile(request : StorageGroupFileNameRequest) : Observable<string> {
    let params = new HttpParams()
      .set("StorageGroup", request.StorageGroup)
      .set("FileName", request.FileName);
    return this.httpClient.get<string>('./Content/GetFile', {params});
  }

  public GetFileList(request : StorageGroupRequest) : Observable<GetFileListResponse> {
    let params = new HttpParams()
      .set("StorageGroup", request.StorageGroup)
    return this.httpClient.get<GetFileListResponse>('./Content/GetFileList');
  }

  public GetHash(request : StorageGroupFileNameRequest) : Observable<GetHashResponse> {
    let params = new HttpParams()
      .set("StorageGroup", request.StorageGroup)
      .set("FileName", request.FileName);
    return this.httpClient.get<GetHashResponse>('./Content/GetHash', {params});
  }

  public GetImageFile(request : GetImageFileRequest) : Observable<string> {
    let params = new HttpParams()
      .set("StorageGroup", request.StorageGroup)
      .set("FileName", request.FileName)
      .set("Width", request.Width)
      .set("Height", request.Height);
    return this.httpClient.get<string>('./Content/GetImageFile', {params});
  }

  public GetMusic(Id : number) : Observable<string> {
    let params = new HttpParams()
      .set("Id", Id);
    return this.httpClient.get<string>('./Content/GetMusic', {params});
  }

  public GetPreviewImage(request : GetPreviewImageRequest) : Observable<string> {
    let params = new HttpParams()
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime)
      .set("Width", request.Width)
      .set("Height", request.Height)
      .set("SecsIn", request.SecsIn)
      .set("Format", request.Format);
    return this.httpClient.get<string>('./Content/GetPreviewImage', {params});
  }

  public GetProgramArtworkList(request : GetProgramArtworkListRequest) : Observable<ArtworkInfoList> {
    let params = new HttpParams()
      .set("Inetref", request.Inetref)
      .set("Season", request.Season);
    return this.httpClient.get<ArtworkInfoList>('./Content/GetProgramArtworkList', {params});
  }

  public GetRecording(request : GetRecordingRequest) : Observable<string> {
    let params = new HttpParams()
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime);
    return this.httpClient.get<string>('./Content/GetRecording', {params});
  }

  public GetRecordingArtwork(request : GetRecordingArtworkRequest) : Observable<string> {
    let params = new HttpParams()
      .set("Type", request.Type)
      .set("Inetref", request.Inetref)
      .set("Season", request.Season)
      .set("Width", request.Width)
      .set("Height", request.Height);
    return this.httpClient.get<string>('./Content/GetRecordingArtwork', {params});
  }

  public GetRecordingArtworkList(request : GetRecordingArtworkListRequest) : Observable<ArtworkInfoList> {
    let params = new HttpParams()
      .set("RecordedId", request.RecordedId)
      .set("ChanId", request.ChanId)
      .set("StartTime", request.StartTime);
    return this.httpClient.get<ArtworkInfoList>('./Content/GetRecordingArtworkList', {params});
  }

  public GetVideo(Id : number) : Observable<string> {
    let params = new HttpParams()
      .set("Id", Id);
    return this.httpClient.get<string>('./Content/GetVideo', {params});
  }

  public GetVideoArtwork(request : GetVideoArtworkRequest) : Observable<string> {
    let params = new HttpParams()
      .set("Type", request.Type)
      .set("Id", request.Id)
      .set("Width", request.Width)
      .set("Height", request.Height);
    return this.httpClient.get<string>('./Content/GetVideoArtwork', {params});
  }
}
