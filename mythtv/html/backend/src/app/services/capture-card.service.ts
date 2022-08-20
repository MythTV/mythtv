import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { CaptureCard, CaptureCardList, CaptureDeviceList, CardTypeList, DiseqcParm, DiseqcTree, DiseqcTreeList } from './interfaces/capture-card.interface';
import { BoolResponse } from './interfaces/common.interface';

@Injectable({
  providedIn: 'root'
})
export class CaptureCardService {

  diseqcTypes: DiseqcParm[] = [
    { description: "Switch", type: "switch", inactive: false },
    { description: "Rotor", type: "rotor", inactive: false },
    { description: "Unicable", type: "scr", inactive: false },
    { description: "LNB", type: "lnb", inactive: false }
  ];

  constructor(private httpClient: HttpClient) {

  }

  public GetCaptureCardList(HostName: string, CardType: string): Observable<CaptureCardList> {
    let params = new HttpParams()
      .set("HostName", HostName)
      .set("CardType", CardType);
    return this.httpClient.get<CaptureCardList>('/Capture/GetCaptureCardList', { params });
  }

  public GetCardTypeList(): Observable<CardTypeList> {
    return this.httpClient.get<CardTypeList>('/Capture/GetCardTypeList', {});
  }

  public UpdateCaptureCard(Cardid: number, Setting: string, Value: string): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Capture/UpdateCaptureCard',
      { Cardid: Cardid, Setting: Setting, Value: Value });
  }

  public DeleteCaptureCard(Cardid: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Capture/RemoveCardInput',
      { CardInputId: Cardid });
  }

  public AddCaptureCard(card: CaptureCard): Observable<number> {
    return this.httpClient.post<number>('/Capture/AddCaptureCard',
      card);
  }

  public GetCaptureDeviceList(CardType: string): Observable<CaptureDeviceList> {
    let params = new HttpParams()
      .set("CardType", CardType);
    return this.httpClient.get<CaptureDeviceList>('/Capture/GetCaptureDeviceList',
      { params });
  }

  public GetDiseqcTreeList(): Observable<DiseqcTreeList> {
    return this.httpClient.get<DiseqcTreeList>('/Capture/GetDiseqcTreeList', {});
  }

  public AddDiseqcTree(tree: DiseqcTree): Observable<number> {
    return this.httpClient.post<number>('/Capture/AddDiseqcTree',
      tree);
  }

  public UpdateDiseqcTree(tree: DiseqcTree): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Capture/UpdateDiseqcTree',
      tree);
  }

  public DeleteDiseqcTree(DiseqcId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('/Capture/RemoveDiseqcTree',
      { DiseqcId: DiseqcId });
  }


}
