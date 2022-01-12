import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';

import { MythHostName, MythTimeZone, MythConnectionInfo, Database, GetSettingRequest, GetSettingResponse, PutSettingRequest, PutSettingResponse } from './interfaces/myth.interface';

const httpOptions = {
  headers: new HttpHeaders({
    'Content-Type':  'application/json'
  })
};

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

  public GetConnectionInfo() : Observable<MythConnectionInfo> {
    return this.httpClient.get<MythConnectionInfo>('/Myth/GetConnectionInfo');
  }

  public GetSetting(setting : GetSettingRequest) : Observable<GetSettingResponse> {
    let params = new HttpParams()
      .set("HostName", setting.HostName)
      .set("Key", setting.Key)
      .set("Default", (setting.Default) ? setting.Default : "");
    return this.httpClient.get<GetSettingResponse>('/Myth/GetSetting', {params})
  }

  public PutSetting(setting: PutSettingRequest) : Observable<PutSettingResponse> {
    return this.httpClient.post<PutSettingResponse>('/Myth/PutSetting', setting)
  }

}
