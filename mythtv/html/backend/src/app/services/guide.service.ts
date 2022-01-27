import { Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';

import { ProgramGuide } from './interfaces/programguide.interface';
import { GetProgramGuideRequest } from './interfaces/guide.interface';

@Injectable({
  providedIn: 'root'
})
export class GuideService {
  startDate : number = Date.now();
  guide_data$! : ProgramGuide;

  toTime(date : string) {
      let d = new Date(date);
      return d.toISOString();
  }
  toStartTime(date : string) {
      return this.toTime(date);
  }
  toEndTime(date : string) {
      let d = new Date(date);
      //let tomorrow = new Date(d.getTime()+86400000);
      let endAt = new Date(d.getTime()+7200000);
      return this.toTime(endAt.toISOString());
  }
  toHalfHour(date : number) {
      let d = new Date(date);
      d.setMinutes((d.getMinutes() < 30) ? 0 : 30);
      d.setSeconds(0);
      return d;
  }

  constructor(private httpClient: HttpClient) { }

  public GetProgramGuide() : Observable<ProgramGuide> {
    let time : string = this.toHalfHour(this.startDate).toISOString();
    let params : GetProgramGuideRequest = {
      "StartTime": this.toStartTime(time),
      "EndTime": this.toEndTime(time),
      "Details": "true",
    };
    return this.httpClient.post<ProgramGuide>('/Guide/GetProgramGuide', params);
  }
}
