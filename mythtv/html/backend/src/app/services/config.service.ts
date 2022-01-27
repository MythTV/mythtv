import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders, HttpParams, HttpErrorResponse } from '@angular/common/http';
import { Observable, throwError } from 'rxjs';
import { catchError, tap } from 'rxjs/operators';
import { MythConnectionInfo, Database } from './interfaces/myth.interface';
import { MythDatabaseStatus, MythCountryList, MythLanguageList, Country, Language } from './interfaces/config.interface';

@Injectable({
    providedIn: 'root'
})
export class ConfigService {

     constructor(private httpClient: HttpClient) { }

    public GetDatabaseStatus(): Observable<MythDatabaseStatus> {
        return this.httpClient.get<MythDatabaseStatus>('/Config/GetDatabaseStatus')
    }

    public SetDatabaseCredentials(data: Database): Observable<Database> {
        return this.httpClient.post<Database>('/Config/SetDatabaseCredentials', data)
    }
    public GetCountries(): Observable<MythCountryList> {
        return this.httpClient.get<MythCountryList>('/Config/GetCountries')
    }

    public GetLanguages(): Observable<MythLanguageList> {
        return this.httpClient.get<MythLanguageList>('/Config/GetLanguages')
    }
}

