import { HttpClient, HttpHeaders, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { CallsignLookupResponse } from '../interfaces/iconlookup.interface';

@Injectable({
  providedIn: 'root'
})
export class IconlookupService {
  services_url = 'https://services.mythtv.org/channel-icon/';

  constructor(private httpClient : HttpClient) { }

  public lookupByCallsign(callsign : string) : Observable<CallsignLookupResponse> {
    let params = new HttpParams().set("callsign", callsign);
    let headers = new HttpHeaders().set("Accept", "application/json");
    return this.httpClient.post<CallsignLookupResponse>(this.services_url + 'lookup', params, {headers});
  }
}
