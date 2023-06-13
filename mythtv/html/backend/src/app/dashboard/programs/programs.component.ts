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

  formatDate(date: string): string {
    if (!date)
      return '';
    if (date.length == 10)
      date = date + ' 00:00';
    return new Date(date).toLocaleDateString()
  }

  formatTime(date: string): string {
    if (!date)
      return '';
    // Get the locale specific time and remove the seconds
    const t = new Date(date);
    const tWithSecs = t.toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  getDuration(program: ScheduleOrProgram): number {
    const starttm = new Date(program.Recording.StartTs).getTime();
    const endtm = new Date(program.Recording.EndTs).getTime();
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  updateRecRule(program: ScheduleOrProgram) {
    let copyProgram = Object.assign({}, program);
    if (this.inter.sched)
      this.inter.sched.open(copyProgram);
  }

}
