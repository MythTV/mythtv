import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { CaptureCardList } from './interfaces/capture-card.interface';
import { BoolResponse } from './interfaces/common.interface';

@Injectable({
  providedIn: 'root'
})
export class CaptureCardService {

  constructor(private httpClient: HttpClient) {

  }

  public GetCaptureCardList(HostName: string, CardType: string) : Observable<CaptureCardList> {
    let params = new HttpParams()
      .set("HostName", HostName)
      .set("CardType", CardType);
    return this.httpClient.get<CaptureCardList>('/Capture/GetCaptureCardList', {params});
  }

  public UpdateCaptureCard(Cardid: number, Setting: string, Value: string) : Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Capture/UpdateCaptureCard',
      {Cardid: Cardid, Setting: Setting, Value: Value});
  }


}
