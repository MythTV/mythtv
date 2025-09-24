import { Component, Input, OnInit } from '@angular/core';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { GuideComponent } from '../../guide.component';
import { DataService } from 'src/app/services/data.service';
import { Channel } from 'src/app/services/interfaces/channel.interface';
import { UtilityService } from 'src/app/services/utility.service';
import { TranslateService } from '@ngx-translate/core';

@Component({
  selector: 'app-guide-programentry',
  templateUrl: './programentry.component.html',
  styleUrls: ['./programentry.component.css']
})
export class ProgramEntryComponent implements OnInit {
  @Input() program!: ScheduleOrProgram;
  @Input() channel!: Channel;
  @Input() guideStartTime!: string;
  @Input() guideEndTime!: string;
  @Input() guideComponent!: GuideComponent;
  @Input() runningTime!: Date;

  dummyProgram?: ScheduleOrProgram;

  editSchedule: boolean = false;
  typeclass = '';
  catclass = ''
  regex = /[^a-z0-9]/g;

  constructor(public dataService: DataService, private utility: UtilityService,
    private translate: TranslateService) {
  }

  ngOnInit(): void {
    this.typeclass = 'guide_type_' + this.program.CatType;
    this.catclass = 'guide_cat_'
      + this.program.Category.toLowerCase().replace(this.regex, '_');

    let guideStart = new Date(this.guideStartTime);
    let progStart = new Date(this.program.StartTime);
    if (this.runningTime.getTime() == 0 || this.runningTime > progStart)
      this.runningTime.setTime(guideStart.getTime());
    // If there is a gap in the guide create a dummy entry
    if (this.runningTime < progStart)
      this.dummyProgram = this.getDummyProgram(this.runningTime, progStart);
    let progEnd = new Date(this.program.EndTime)
    this.runningTime.setTime(progEnd.getTime());
}

  durationToWidth(program:ScheduleOrProgram ): number {
    let p_start = new Date(program.StartTime);
    let p_end = new Date(program.EndTime);
    let w_start = new Date(this.guideStartTime);
    let w_end = new Date(this.guideEndTime);
    let beginsBefore = p_start < w_start;
    let endsAfter = p_end > w_end;
    let startPoint = (beginsBefore ? w_start : p_start);
    let endPoint = (endsAfter ? w_end : p_end);
    let timeWindow = w_end.getTime() - w_start.getTime();
    let programWindow = endPoint.getTime() - startPoint.getTime();
    let program_width = ((programWindow / timeWindow) * 100);
    return program_width;
  }

  getDummyProgram(startTime: Date, endTime: Date): ScheduleOrProgram {
    let dummy = <ScheduleOrProgram> <unknown> {
      CatType: 'default',
      Category: this.translate.instant('dashboard.guide.unknown'),
      Title: this.translate.instant('dashboard.guide.unknown'),
      StartTime: startTime.toISOString(),
      EndTime: endTime.toISOString()
    };
    return dummy;
  }

  openDialog() {
    if (this.guideComponent.inter.sched)
      this.guideComponent.inter.sched.open(this.program, this.channel);
  }

  formatAirDate(): string {
    if (!this.program.Airdate)
      return '';
    let date = this.program.Airdate + ' 00:00';
    return this.utility.formatDate(date, false);
  }

  getToolTip() {
    let ret = '';
    if (this.program.Airdate)
      ret = this.translate.instant('dashboard.recordings.orig_airdate') + ' ' + this.formatAirDate() + '\n';
    return ret + this.program.Description;
  }

}
