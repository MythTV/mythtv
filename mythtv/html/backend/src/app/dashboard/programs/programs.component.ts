import { Component, Input, OnInit } from '@angular/core';
import { ScheduleLink } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { DvrService } from 'src/app/services/dvr.service';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { RecRule } from 'src/app/services/interfaces/recording.interface';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-programs',
  templateUrl: './programs.component.html',
  styleUrls: ['./programs.component.css']
})
export class ProgramsComponent implements OnInit {
  @Input() programs: ScheduleOrProgram[] = [];
  @Input() inter!: ScheduleLink;

  displayStop = false;
  successCount = 0;
  errorCount = 0;
  program?: ScheduleOrProgram;
  regex = /[^a-z0-9]/g;

  constructor(public dataService: DataService, private dvrService: DvrService,
    private utility: UtilityService) {
  }

  ngOnInit(): void { }

  formatStartDate(program: ScheduleOrProgram): string {
    return this.utility.formatDate(program.StartTime, true);
  }

  formatAirDate(program: ScheduleOrProgram): string {
    if (!program.Airdate)
      return '';
    let date = program.Airdate + ' 00:00';
    return this.utility.formatDate(date, true);
  }


  formatStartTime(program: ScheduleOrProgram): string {
    let starttm = new Date(program.StartTime).getTime();
    const tWithSecs = new Date(starttm).toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  getClasses(program: ScheduleOrProgram): string[] {
    let typeclass = 'guide_type_' + program.CatType;
    let catclass = 'guide_cat_'
      + program.Category.toLowerCase().replace(this.regex, '_');
    return [typeclass, catclass, 'guide_type_default'];
  }

  getDuration(program: ScheduleOrProgram): number {
    let  starttm = new Date(program.StartTime).getTime();
    let  endtm = new Date(program.EndTime).getTime();
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  updateRecRule(program: ScheduleOrProgram) {
    if (this.inter.sched)
      this.inter.sched.open(program);
  }

  override(program: ScheduleOrProgram) {
    if (this.inter.sched) {
      if (program.Recording.RecType == 7) // If already an override
        this.inter.sched.open(program);
      else
        this.inter.sched.open(program, undefined, <RecRule>{ Type: 'Override Recording' });
    }
  }

  stopRequest(program: ScheduleOrProgram) {
    if (program.Recording.RecordId) {
      this.program = program;
      this.displayStop = true;
    }
  }

  stopRecording(program: ScheduleOrProgram) {
    this.errorCount = 0;
    this.dvrService.StopRecording(program.Recording.RecordedId)
      .subscribe({
        next: (x) => {
          if (x.bool) {
            this.displayStop = false;
            setTimeout(() => this.inter.summaryComponent.refresh(), 3000);
          }
          else
            this.errorCount++;
        },
        error: (err) => {
          this.errorCount++;
        }

      });
  }

}
