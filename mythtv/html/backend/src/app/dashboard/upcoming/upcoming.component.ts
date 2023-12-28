import { Component, OnInit } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { LazyLoadEvent, MessageService } from 'primeng/api';
import { ScheduleLink, SchedulerSummary } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { DvrService } from 'src/app/services/dvr.service';
import { GetUpcomingRequest } from 'src/app/services/interfaces/dvr.interface';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { RecRule } from 'src/app/services/interfaces/recording.interface';
import { UtilityService } from 'src/app/services/utility.service';

interface RuleListEntry {
  Id: number;
  Title: string;
}


@Component({
  selector: 'app-upcoming',
  templateUrl: './upcoming.component.html',
  styleUrls: ['./upcoming.component.css'],
  providers: [MessageService]
})
export class UpcomingComponent implements OnInit, SchedulerSummary {

  programs: ScheduleOrProgram[] = [];
  recRules: RuleListEntry[] = [];
  allRecRules: RuleListEntry[] = [];
  activeRecRules: RuleListEntry[] = [];
  defaultRecRule: RuleListEntry = { Id: 0, Title: 'settings.chanedit.all' };
  editingProgram?: ScheduleOrProgram;
  displayUpdateDlg = false;
  showAllStatuses = false;
  refreshing = false;
  loaded = false;
  inter: ScheduleLink = { summaryComponent: this };
  lazyLoadEvent!: LazyLoadEvent;

  displayStop = false;
  errorCount = 0;
  program?: ScheduleOrProgram;

  constructor(private dvrService: DvrService, private messageService: MessageService,
    private translate: TranslateService, public dataService: DataService,
    private utility: UtilityService) {
    this.translate.get(this.defaultRecRule.Title).subscribe(data => this.defaultRecRule.Title = data);
    this.loadRecRules();
  }

  ngOnInit(): void { }

  refresh() {
    this.refreshing = true;
    this.loadRecRules();
    this.loadLazy(this.lazyLoadEvent);
  }

  loadRecRules() {
    this.dvrService.GetRecordScheduleList({}).subscribe({
      next: (data) => {
        this.allRecRules.length = 0;
        this.allRecRules.push(this.defaultRecRule);
        this.activeRecRules.length = 0;
        this.activeRecRules.push(this.defaultRecRule);
        data.RecRuleList.RecRules.forEach((entry) => {
          if (entry.Type != 'Recording Template') {
            let recRule = {
              Id: entry.Id,
              Title: entry.Title.substring(0, 30) + ' [' + this.utility.recTypeTrans[entry.Type] + ']'
            };
            this.allRecRules.push(recRule);
            if (entry.NextRecording) {
              this.activeRecRules.push(recRule);
            };
          }
        });
      },
    });
  }

  loadLazy(event: LazyLoadEvent) {
    this.lazyLoadEvent = event;

    let request: GetUpcomingRequest = {
      StartIndex: 0,
      Count: 1
    };
    if (event.first)
      request.StartIndex = event.first;
    if (event.rows) {
      request.Count = event.rows;
      // When it only requests 50 rows, page down waits until the entire
      // screen is empty before loading the next page. Fix this by always
      // requesting at least 100 records.
      if (request.Count < 100)
        request.Count = 100;
    }
    if (event.filters) {
      if (event.filters.ShowAll.value) {
        request.ShowAll = true;
      }
      if (event.filters.RecordId.value) {
        request.RecordId = event.filters.RecordId.value;
      }
    }
    this.recRules.length = 0;
    if (request.ShowAll)
      this.recRules.push(...this.allRecRules)
    else
      this.recRules.push(...this.activeRecRules)
    this.dvrService.GetUpcomingList(request).subscribe(data => {
      let recordings = data.ProgramList;
      this.programs.length = data.ProgramList.TotalAvailable;
      // populate page of virtual programs
      // note that Count is returned as the count requested, even
      // if less items are returned because you hit the end.
      // Maybe we should use recordings.Programs.length
      this.programs.splice(recordings.StartIndex, recordings.Count,
        ...recordings.Programs);
      // notify of change
      this.programs = [...this.programs]
      this.refreshing = false;
    });

  }

  formatStartDate(program: ScheduleOrProgram): string {
    return this.utility.formatDate(program.Recording.StartTs, true);
  }

  formatAirDate(program: ScheduleOrProgram): string {
    if (!program.Airdate)
      return '';
    let date = program.Airdate + ' 00:00';
    return this.utility.formatDate(date, true);
  }

  formatStartTime(program: ScheduleOrProgram): string {
    const tWithSecs = new Date(program.Recording.StartTs).toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  getDuration(program: ScheduleOrProgram): number {
    let starttm = new Date(program.Recording.StartTs).getTime();
    let endtm = new Date(program.Recording.EndTs).getTime();
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
