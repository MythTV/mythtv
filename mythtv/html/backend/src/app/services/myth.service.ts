import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

import { MythHostName, MythTimeZone, MythConnectionInfo, Database } from './interfaces/myth.interface';

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
}
