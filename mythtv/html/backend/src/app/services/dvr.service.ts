import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { GetRecStorageGroupListResponse, GetUpcomingListResponse } from './interfaces/dvr.interface';

@Injectable({
  providedIn: 'root'
})
export class DvrService {

  constructor(private httpClient: HttpClient) { }

  public GetRecStorageGroupList() : Observable<GetRecStorageGroupListResponse> {
    return this.httpClient.get<GetRecStorageGroupListResponse>('/Dvr/GetRecStorageGroupList');
  }

  public GetUpcomingList() : Observable<GetUpcomingListResponse> {
    return this.httpClient.get<GetUpcomingListResponse>('/Dvr/GetUpcomingList');
  }
}
