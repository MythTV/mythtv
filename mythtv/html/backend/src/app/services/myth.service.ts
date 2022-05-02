import { Injectable } from '@angular/core';
import { HttpClient, HttpParams} from '@angular/common/http';
import { Observable, throwError } from 'rxjs';
import {
  MythHostName,
  MythTimeZone,
  MythConnectionInfo,
  Database,
  GetSettingRequest,
  GetSettingResponse,
  PutSettingRequest,
  GetStorageGroupDirsRequest,
  GetStorageGroupDirsResponse,
  AddStorageGroupDirRequest,
} from './interfaces/myth.interface';
import { BoolResponse } from './interfaces/common.interface';

@Injectable({
  providedIn: 'root'
})
export class MythService {

  constructor(private httpClient: HttpClient) { }

  public GetHostName() : Observable<MythHostName> {
    return this.httpClient.get<MythHostName>('/Myth/GetHostName');
  }

  public GetTimeZone() : Observable<MythTimeZone> {
    return this.httpClient.get<MythTimeZone>('/Myth/GetTimeZone');
  }

  public GetConnectionInfo(Pin : string) : Observable<MythConnectionInfo> {
    let params = new HttpParams()
      .set("Pin", Pin);
    return this.httpClient.get<MythConnectionInfo>('/Myth/GetConnectionInfo', {params});
   }

  public GetSetting(setting : GetSettingRequest) : Observable<GetSettingResponse> {
    let params = new HttpParams()
      .set("HostName", setting.HostName)
      .set("Key", setting.Key)
      .set("Default", (setting.Default) ? setting.Default : "");
    return this.httpClient.get<GetSettingResponse>('/Myth/GetSetting', {params})
  }

  public PutSetting(setting: PutSettingRequest) : Observable<BoolResponse> {
    console.log(setting);
    return this.httpClient.post<BoolResponse>('/Myth/PutSetting', setting)
  }

  public SetConnectionInfo(data: Database) : Observable<BoolResponse> {
    console.log("SetConnectionInfo :-" + data.Name);
    return this.httpClient.post<BoolResponse>('/Myth/SetConnectionInfo', data)
  }

  public GetStorageGroupDirs(request? : GetStorageGroupDirsRequest) : Observable<GetStorageGroupDirsResponse> {
    if ((typeof request !== 'undefined') &&
       ((typeof request.GroupName !== 'undefined') || (typeof request.HostName !== 'undefined'))){
      return this.httpClient.post<GetStorageGroupDirsResponse>('/Myth/GetStorageGroupDirs', request);
    } else {
      return this.httpClient.get<GetStorageGroupDirsResponse>('/Myth/GetStorageGroupDirs');
    }
  }

  public AddStorageGroupDir(request: AddStorageGroupDirRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Myth/AddStorageGroupDir', request);
  }

  // public RemoveStorageGroupDir(data: StorageGroupRequest) : Observable<BoolResponse> {
  //   console.log(data);
  //   let params = new HttpParams()
  //     .set("HostName", data.HostName)
  //     .set("GroupName", data.GroupName)
  //     .set("DirName", data.DirName);
  //   return this.httpClient.post<BoolResponse>('/Myth/RemoveStorageGroupDir', {params})
  // }
}
