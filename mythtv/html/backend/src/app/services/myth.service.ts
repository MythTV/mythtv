import { Injectable } from '@angular/core';
import { HttpClient, HttpParams} from '@angular/common/http';
import { Observable } from 'rxjs';
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
  ManageDigestUserRequest,
  ManageUrlProtectionRequest,
  RemoveStorageGroupRequest,
  TestDBSettingsRequest
} from './interfaces/myth.interface';
import { BoolResponse, StringResponse } from './interfaces/common.interface';
import { BackendInfo } from './interfaces/backend.interface';
import { FrontendList } from './interfaces/frontend.interface';

@Injectable({
  providedIn: 'root'
})
export class MythService {

  constructor(private httpClient: HttpClient) { }

  public AddStorageGroupDir(request: AddStorageGroupDirRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/AddStorageGroupDir', request);
  }

  public BackupDatabase() : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/BackupDatabase', {body: undefined});
  }

  public CheckDatabase(Repair : boolean) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/CheckDatabase', Repair);
  }

  public DelayShutdown() : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('Myth/DelayShutdown', {body: undefined});
  }

  public GetBackendInfo() : Observable<BackendInfo> {
    return this.httpClient.get<BackendInfo>('./Myth/GetBackendInfo');
  }

  public GetConnectionInfo(Pin : string) : Observable<MythConnectionInfo> {
    let params = new HttpParams()
      .set("Pin", Pin);
    return this.httpClient.get<MythConnectionInfo>('./Myth/GetConnectionInfo', {params});
  }

  public GetFrontends(OnLine : boolean) : Observable<FrontendList> {
    let params = new HttpParams()
      .set("OnLine", OnLine);
    return this.httpClient.get<FrontendList>('./Myth/GetFrontends', {params});
  }

  public GetHostName() : Observable<MythHostName> {
    return this.httpClient.get<MythHostName>('./Myth/GetHostName');
  }

  public GetHosts() : Observable<{StringList:string[]}> {
    return this.httpClient.get<{StringList:string[]}>('./Myth/GetHosts');
  }

  public GetKeys() : Observable<String[]> {
    return this.httpClient.get<String[]>('./Myth/GetKeys');
  }

  public GetSetting(setting : GetSettingRequest) : Observable<GetSettingResponse> {
    let params = new HttpParams()
      .set("HostName", (setting.HostName) ? setting.HostName : "")
      .set("Key", setting.Key)
      .set("Default", (setting.Default) ? setting.Default : "");
    return this.httpClient.get<GetSettingResponse>('./Myth/GetSetting', {params})
  }

  public GetSettingList(hostname : string) : Observable<SettingList> {
    let params = new HttpParams()
      .set("HostName", hostname);
    return this.httpClient.get<SettingList>('./Myth/GetSettingList', {params})
  }

  public GetStorageGroupDirs(request? : GetStorageGroupDirsRequest) : Observable<GetStorageGroupDirsResponse> {
    if ((typeof request !== 'undefined') &&
       ((typeof request.GroupName !== 'undefined') || (typeof request.HostName !== 'undefined'))){
      return this.httpClient.post<GetStorageGroupDirsResponse>('./Myth/GetStorageGroupDirs', request);
    } else {
      return this.httpClient.get<GetStorageGroupDirsResponse>('./Myth/GetStorageGroupDirs');
    }
  }

  public GetDirListing(DirName : string, Files? : boolean) : Observable<{DirListing: String[]}> {
    let params = new HttpParams()
      .set("DirName", DirName);
      if (Files)
        params = params.set("Files", Files)
    return this.httpClient.get<{DirListing: String[]}>('./Myth/GetDirListing', {params});
  }

  public GetTimeZone() : Observable<MythTimeZone> {
    return this.httpClient.get<MythTimeZone>('./Myth/GetTimeZone');
  }

  public ManageDigestUser(request : ManageDigestUserRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/ManageDigestUser', request);
  }

  public ManageUrlProtection(request : ManageUrlProtectionRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/ManageUrlProtection', request);
  }

  public ProfileDelete() : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/ProfileDelete', {body : undefined});
  }

  public ProfileSubmit() : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/ProfileSubmit', {body : undefined});
  }

  public ProfileText() : Observable<string> {
    return this.httpClient.get<string>('./Myth/ProfileText');
  }

  public ProfileURL() : Observable<string> {
    return this.httpClient.get<string>('./Myth/ProfileURL');
  }

  public ProfileUpdated() : Observable<string> {
    return this.httpClient.get<string>('./Myth/ProfileUpdated');
  }

  public PutSetting(setting: PutSettingRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/PutSetting', setting)
  }

  public DeleteSetting(setting: {HostName: string, Key: string}) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/DeleteSetting', setting)
  }

  public RemoveStorageGroupDir(request: RemoveStorageGroupRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/RemoveStorageGroupDir', request);
  }

  public SetConnectionInfo(data: Database) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/SetConnectionInfo', data)
  }

  public TestDBSettings(request : TestDBSettingsRequest) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/TestDBSettings', request);
  }

  public ManageScheduler(request: {Enable?: boolean, Disable?: boolean}) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/ManageScheduler', request);
  }

  public Shutdown(request: {Retcode?: number, Restart?: boolean, WebOnly?: boolean}) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Myth/Shutdown', request);
  }

  public Proxy(url: String) : Observable<StringResponse> {
    return this.httpClient.post<StringResponse>('./Myth/Proxy', {Url: url});
  }

  public LoginUser(UserName: string, Password: string) : Observable<StringResponse> {
    return this.httpClient.post<StringResponse>('./Myth/LoginUser', {UserName: UserName, Password: Password});
  }

  public GetUsers() : Observable<{StringList:string[]}> {
    return this.httpClient.get<{StringList:string[]}>('./Myth/GetUsers');
  }

}
