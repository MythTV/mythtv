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
  SettingList,
} from './interfaces/myth.interface';
import { BoolResponse } from './interfaces/common.interface';
import { BackendInfo } from './interfaces/backend.interface';
import { FrontendList } from './interfaces/frontend.interface';

@Injectable({
  providedIn: 'root'
})
export class MythService {

  constructor(private httpClient: HttpClient) { }

  public AddStorageGroupDir(request: AddStorageGroupDirRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Myth/AddStorageGroupDir', request);
  }

  public BackupDatabase() : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Myth/BackupDatabase', {body: undefined});
  }

  public CheckDatabase(Repair : boolean) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Myth/CheckDatabase', Repair);
  }

  public DelayShutdown() : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('Myth/DelayShutdown', {body: undefined});
  }

  public GetBackendInfo() : Observable<BackendInfo> {
    return this.httpClient.get<BackendInfo>('/Myth/GetBackendInfo');
  }

  public GetConnectionInfo(Pin : string) : Observable<MythConnectionInfo> {
    let params = new HttpParams()
      .set("Pin", Pin);
    return this.httpClient.get<MythConnectionInfo>('/Myth/GetConnectionInfo', {params});
  }

  public GetFrontends(OnLine : boolean) : Observable<FrontendList> {
    let params = new HttpParams()
      .set("OnLine", OnLine);
    return this.httpClient.get<FrontendList>('/Myth/GetFrontends', {params});
  }

  public GetHostName() : Observable<MythHostName> {
    return this.httpClient.get<MythHostName>('/Myth/GetHostName');
  }

  public GetHosts() : Observable<String[]> {
    return this.httpClient.get<String[]>('/Myth/GetHosts');
  }

  public GetKeys() : Observable<String[]> {
    return this.httpClient.get<String[]>('/Myth/GetKeys');
  }

  public GetSetting(setting : GetSettingRequest) : Observable<GetSettingResponse> {
    let params = new HttpParams()
      .set("HostName", setting.HostName)
      .set("Key", setting.Key)
      .set("Default", (setting.Default) ? setting.Default : "");
    return this.httpClient.get<GetSettingResponse>('/Myth/GetSetting', {params})
  }

  public GetSettingList(hostname : string) : Observable<SettingList> {
    let params = new HttpParams()
      .set("HostName", hostname);
    return this.httpClient.get<SettingList>('/Myth/GetSettingList', {params})
  }

  public GetTimeZone() : Observable<MythTimeZone> {
    return this.httpClient.get<MythTimeZone>('/Myth/GetTimeZone');
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

  // public RemoveStorageGroupDir(data: StorageGroupRequest) : Observable<BoolResponse> {
  //   console.log(data);
  //   let params = new HttpParams()
  //     .set("HostName", data.HostName)
  //     .set("GroupName", data.GroupName)
  //     .set("DirName", data.DirName);
  //   return this.httpClient.post<BoolResponse>('/Myth/RemoveStorageGroupDir', {params})
  // }
}
