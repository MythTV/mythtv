import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

import { BackendStatusResponse, BackupsList, StatsResponse } from './interfaces/status.interface';

@Injectable({
    providedIn: 'root'
})
export class StatusService {

    constructor(private httpClient: HttpClient) { }

    public GetBackendStatus(): Observable<BackendStatusResponse> {
        return this.httpClient.get<BackendStatusResponse>('./Status/GetBackendStatus');
    }

    public GetRecStats(): Observable<StatsResponse> {
        return this.httpClient.get<StatsResponse>('./Status/GetRecStats');
    }

    public GetBackupsList(): Observable<BackupsList> {
        return this.httpClient.get<BackupsList>('./Status/GetBackupsList');
    }

}
