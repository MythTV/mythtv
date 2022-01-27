import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders, HttpParams, HttpErrorResponse} from '@angular/common/http';
import { Observable, throwError } from 'rxjs';
import { catchError } from 'rxjs/operators';
import { MythHostName, MythTimeZone, MythConnectionInfo, Database, GetSettingRequest, GetSettingResponse, PutSettingRequest, PutSettingResponse } from './interfaces/myth.interface';

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
    console.log(setting);
    return this.httpClient.post<PutSettingResponse>('/Myth/PutSetting', setting)
  }

  public SetConnectionInfo(data: Database) : Observable<PutSettingResponse> {
    console.log("SetConnectionInfo :-" + data.Name);  
    return this.httpClient.post<PutSettingResponse>('/Myth/SetConnectionInfo', data)
  }
}
