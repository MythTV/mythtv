import { Component, OnInit } from '@angular/core';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';
import { GuideService } from 'src/app/services/guide.service';
import { Channel } from '../services/interfaces/channel.interface';
import { ProgramGuide } from 'src/app/services/interfaces/programguide.interface';
import { TranslateService, TranslationChangeEvent } from '@ngx-translate/core';

@Component({
  selector: 'app-guide',
  templateUrl: './guide.component.html',
  styleUrls: ['./guide.component.css']
})
export class GuideComponent implements OnInit {
  m_guideData$!:  Observable<ProgramGuide>;
  m_startDate:    Date = new Date();
  m_endDate:      Date = new Date();
  m_dateFormat:   string = ''
  m_channelData:  Channel[] = [];
  m_channelTotal: number = 10;
  m_rows:         number = 10;

  constructor(private guideService : GuideService,
              private translate : TranslateService) {

    this.setDateFormat();

    this.translate.onLangChange.subscribe((event : TranslationChangeEvent) => {
      console.log("Event: language change, new language (" + event.lang + ")");
      this.switchLanguage(event.lang);
      this.fetchData();
    })
  }

  ngOnInit(): void {
    this.fetchData();
  }

  setDateFormat() : void {
    this.translate.get("primeng.dateFormat").subscribe(data => this.m_dateFormat = data);
    console.log("Date format is (" + this.m_dateFormat + ")");
  }

  switchLanguage(language : string) : void {
    this.translate.use(language);
    this.setDateFormat();
  }

  fetchData(reqDate?: Date) : void {
    this.m_guideData$ = this.guideService.GetProgramGuide(reqDate).pipe(
      tap(data =>
        {
          console.log(data);
          this.m_startDate = new Date(data.ProgramGuide.StartTime);
          this.m_endDate = new Date(data.ProgramGuide.EndTime);
          this.m_channelData = data.ProgramGuide.Channels;
          this.m_channelTotal = data.ProgramGuide.TotalAvailable;
        }
      )
    )
  }

  inDisplayWindow(startTime : string, endTime : string) : boolean {
    let p_start = new Date(startTime);
    let p_end   = new Date(endTime);
    let w_start = new Date(this.m_startDate);
    let w_end   = new Date(this.m_endDate);
    if (p_end <= w_start) {
      return false;
    }
    if (p_start >= w_end) {
      return false;
    }
    return (p_start < w_end);
  }

  onDatePickerClose() : void {
    console.log("New time is " + this.m_startDate);
    this.fetchData(this.m_startDate);
  }
}
