import { Component, OnInit } from '@angular/core';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';
import { GuideService } from 'src/app/services/guide.service';
import { Channel } from '../services/interfaces/channel.interface';
import { ProgramGuide } from 'src/app/services/interfaces/programguide.interface';

@Component({
  selector: 'app-guide',
  templateUrl: './guide.component.html',
  styleUrls: ['./guide.component.css']
})
export class GuideComponent implements OnInit {
  m_guideData$!:  Observable<ProgramGuide>;
  m_channelData:  Channel[] = [];
  m_channelTotal: number = 10;
  m_rows:         number = 10;

  constructor(private guideService : GuideService) { }

  ngOnInit(): void {
    this.m_guideData$ = this.guideService.GetProgramGuide().pipe(
      tap(data => console.log(data)),
      tap(data => this.m_channelData = data.ProgramGuide.Channels),
      tap(data => this.m_channelTotal = data.ProgramGuide.TotalAvailable),
    )
  }

  loadData(event: any): void {
    console.log(event);
    this.m_guideData$ = this.guideService.GetProgramGuide().pipe(
      tap(data => console.log(data)),
      tap(data => this.m_channelData = data.ProgramGuide.Channels),
      tap(data => this.m_channelTotal = data.ProgramGuide.TotalAvailable),
    )
  }

  inDisplayWindow(startTime : string, endTime : string) : boolean {
    let p_start = new Date(startTime);
    let w_end   = new Date(endTime);
    return (p_start < w_end);
  }

}
