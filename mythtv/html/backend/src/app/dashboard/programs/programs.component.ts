import { Component, Input, OnInit } from '@angular/core';
import { ScheduleLink } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';

@Component({
  selector: 'app-programs',
  templateUrl: './programs.component.html',
  styleUrls: ['./programs.component.css']
})
export class ProgramsComponent implements OnInit {
  @Input() programs: ScheduleOrProgram[] = [];
  @Input() inter!: ScheduleLink;
  // Usage: GUIDE, UPCOMING
  @Input() usage: string = '';

  // programs!: ProgramList;
  editingProgram?: ScheduleOrProgram;
  displayUpdateDlg = false;
  displayUnsaved = false;
  successCount = 0;
  errorCount = 0;
  showAllStatuses = false;
  refreshing = false;

  constructor(public dataService: DataService) {
  }

  ngOnInit(): void { }

  formatStartDate(program: ScheduleOrProgram): string {
    let starttm;
    if (this.usage == 'UPCOMING') {
      starttm = new Date(program.Recording.StartTs).getTime();
    }
    else {
      starttm = new Date(program.StartTime).getTime();
    }
    return new Date(starttm).toLocaleDateString()
  }

  formatAirDate(program: ScheduleOrProgram): string {
    if (!program.Airdate)
      return '';
    let date = program.Airdate + ' 00:00';
    return new Date(date).toLocaleDateString()
  }


  formatStartTime(program: ScheduleOrProgram): string {
    let starttm;
    if (this.usage == 'UPCOMING') {
      starttm = new Date(program.Recording.StartTs).getTime();
    }
    else {
      starttm = new Date(program.StartTime).getTime();
    }
    const tWithSecs = new Date(starttm).toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  getDuration(program: ScheduleOrProgram): number {
    let starttm;
    let endtm;
    if (this.usage == 'UPCOMING') {
      starttm = new Date(program.Recording.StartTs).getTime();
      endtm = new Date(program.Recording.EndTs).getTime();
    }
    else {
      starttm = new Date(program.StartTime).getTime();
      endtm = new Date(program.EndTime).getTime();
    }
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  updateRecRule(program: ScheduleOrProgram) {
    let copyProgram = Object.assign({}, program);
    if (this.inter.sched)
      this.inter.sched.open(copyProgram);
  }

}
