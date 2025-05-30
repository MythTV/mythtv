import { HttpClient, HttpParams } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { CaptureCard, CaptureCardList, CaptureDeviceList, CardSubType, CardTypeList, DiseqcConfig, DiseqcConfigList, DiseqcParm, DiseqcTree, DiseqcTreeList, InputGroupList } from './interfaces/capture-card.interface';
import { BoolResponse } from './interfaces/common.interface';
import { RecProfileGroupList } from './interfaces/recprofile.interface';

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
    return this.httpClient.get<CaptureCardList>('./Capture/GetCaptureCardList', { params });
  }

  public GetCardTypeList(): Observable<CardTypeList> {
    return this.httpClient.get<CardTypeList>('./Capture/GetCardTypeList', {});
  }

  public UpdateCaptureCard(Cardid: number, Setting: string, Value: string): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/UpdateCaptureCard',
      { Cardid: Cardid, Setting: Setting, Value: Value });
  }

  public DeleteCaptureCard(Cardid: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/RemoveCardInput',
      { CardInputId: Cardid });
  }

  public DeleteAllCaptureCards(): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/RemoveAllCaptureCards' ,
      { });
  }

  public AddCaptureCard(card: CaptureCard): Observable<number> {
    return this.httpClient.post<number>('./Capture/AddCaptureCard',
      card);
  }

  public GetCaptureDeviceList(CardType: string): Observable<CaptureDeviceList> {
    let params = new HttpParams()
      .set("CardType", CardType);
    return this.httpClient.get<CaptureDeviceList>('./Capture/GetCaptureDeviceList',
      { params });
  }

  public GetDiseqcTreeList(): Observable<DiseqcTreeList> {
    return this.httpClient.get<DiseqcTreeList>('./Capture/GetDiseqcTreeList', {});
  }

  public AddDiseqcTree(tree: DiseqcTree): Observable<number> {
    return this.httpClient.post<number>('./Capture/AddDiseqcTree',
      tree);
  }

  public UpdateDiseqcTree(tree: DiseqcTree): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/UpdateDiseqcTree',
      tree);
  }

  public DeleteDiseqcTree(DiseqcId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/RemoveDiseqcTree',
      { DiseqcId: DiseqcId });
  }

  public GetDiseqcConfigList(): Observable<DiseqcConfigList> {
    return this.httpClient.get<DiseqcConfigList>('./Capture/GetDiseqcConfigList', {});
  }

  public AddDiseqcConfig(tree: DiseqcConfig): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/AddDiseqcConfig',
      tree);
  }

  public DeleteDiseqcConfig(CardId: number): Observable<BoolResponse> {
    console.log(CardId)
    return this.httpClient.post<BoolResponse>('./Capture/RemoveDiseqcConfig',
      { CardId: CardId });
  }

  public GetInputGroupList(): Observable<InputGroupList> {
    return this.httpClient.get<InputGroupList>('./Capture/GetUserInputGroupList', {});
  }

  public SetInputMaxRecordings(InputId: number, Max: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/SetInputMaxRecordings',
      { InputId: InputId, Max: Max });
  }

  public AddUserInputGroup(Name: string): Observable<number> {
    return this.httpClient.post<number>('./Capture/AddUserInputGroup',
      { Name: Name });
  }

  public LinkInputGroup(InputId: number, InputGroupId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/LinkInputGroup',
      { InputId: InputId, InputGroupId: InputGroupId });
  }

  public UnlinkInputGroup(InputId: number, InputGroupId: number): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/UnlinkInputGroup',
      { InputId: InputId, InputGroupId: InputGroupId });
  }

  public GetRecProfileGroupList(GroupId: number, ProfileId: number, OnlyInUse: boolean) : Observable<RecProfileGroupList> {
    let params = new HttpParams()
    .set("GroupId", GroupId)
    .set("ProfileId", ProfileId)
    .set("OnlyInUse", OnlyInUse);
    return this.httpClient.get<RecProfileGroupList>('./Capture/GetRecProfileGroupList', {params})
  }

  public AddRecProfile(GroupId: number, ProfileName: string, VideoCodec: string,
     AudioCodec: string): Observable<number> {
    return this.httpClient.post<number>('./Capture/AddRecProfile',
      { GroupId: GroupId, ProfileName: ProfileName, VideoCodec: VideoCodec, AudioCodec: AudioCodec });
  }

  public DeleteRecProfile(ProfileId: number): Observable<BoolResponse> {
   return this.httpClient.post<BoolResponse>('./Capture/DeleteRecProfile',
     { ProfileId: ProfileId });
 }

  public UpdateRecProfile(ProfileId: number, VideoCodec: string, AudioCodec: string): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/UpdateRecProfile',
      { ProfileId: ProfileId, VideoCodec: VideoCodec, AudioCodec: AudioCodec });
  }

  public UpdateRecProfileParam(ProfileId: number, Name: string, Value: string): Observable<BoolResponse> {
    return this.httpClient.post<BoolResponse>('./Capture/UpdateRecProfileParam',
      { ProfileId: ProfileId, Name: Name, Value: Value });
  }

  public GetCardSubType(CardId: number) : Observable<{CardSubType: CardSubType}> {
    let params = new HttpParams().set("cardid",CardId);
    return this.httpClient.get<{CardSubType: CardSubType}>('./Capture/GetCardSubType', {params})
  }
 }
