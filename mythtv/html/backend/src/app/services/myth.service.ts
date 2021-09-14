import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

import { MythHostName } from './myth.interface';

@Injectable({
  providedIn: 'root'
})
export class MythService {

  constructor(private httpClient: HttpClient) { }

  public GetHostName() : Observable<MythHostName> {
    return this.httpClient.get<MythHostName>('http://localhost:7744/Myth/GetHostName');
  }
}
