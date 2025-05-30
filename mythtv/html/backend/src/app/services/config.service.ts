import { Injectable } from '@angular/core';
import { HttpClient,  HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
import { Database } from './interfaces/myth.interface';
import { MythDatabaseStatus, IPAddressList, SystemEventList } from './interfaces/config.interface';
import { MythCountryList } from './interfaces/country.interface';
import { MythLanguageList } from './interfaces/language.interface';
import { BoolResponse } from './interfaces/common.interface';

@Injectable({
    providedIn: 'root'
})
export class ConfigService {

    constructor(private httpClient: HttpClient) { }

    public GetDatabaseStatus(): Observable<MythDatabaseStatus> {
        return this.httpClient.get<MythDatabaseStatus>('./Config/GetDatabaseStatus')
    }

    public SetDatabaseCredentials(data: Database): Observable<BoolResponse> {
        return this.httpClient.post<BoolResponse>('./Config/SetDatabaseCredentials', data)
    }
    public GetCountries(): Observable<MythCountryList> {
        return this.httpClient.get<MythCountryList>('./Config/GetCountries')
    }

    public GetLanguages(): Observable<MythLanguageList> {
        return this.httpClient.get<MythLanguageList>('./Config/GetLanguages')
    }

    public GetIPAddresses(protocol: string): Observable<IPAddressList> {
        let params = new HttpParams().set("Protocol", protocol);
        return this.httpClient.get<IPAddressList>('./Config/GetIPAddresses', { params })
    }

    public GetSystemEvents(host?: string): Observable<SystemEventList> {
        let params = new HttpParams();
        if (host)
            params.set("Host", host);
        return this.httpClient.get<SystemEventList>('./Config/GetSystemEvents', { params })
    }
}

