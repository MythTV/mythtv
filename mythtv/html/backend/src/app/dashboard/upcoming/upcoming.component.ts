import { Component, OnInit } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { MessageService } from 'primeng/api';
import { ScheduleLink, SchedulerSummary } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { DvrService } from 'src/app/services/dvr.service';
import { ProgramList, ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { RecRule } from 'src/app/services/interfaces/recording.interface';

@Component({
  selector: 'app-upcoming',
  templateUrl: './upcoming.component.html',
  styleUrls: ['./upcoming.component.css'],
  providers: [MessageService]
})
export class UpcomingComponent implements OnInit, SchedulerSummary {


  programs: ScheduleOrProgram[] = [];
  recRules: RecRule[] = [];
  editingProgram?: ScheduleOrProgram;
  displayUpdateDlg = false;
  displayUnsaved = false;
  successCount = 0;
  errorCount = 0;
  showAllStatuses = false;
  recordId = 0;
  refreshing = false;
  loaded = false;
  inter: ScheduleLink = { summaryComponent: this };

  constructor(private dvrService: DvrService, private messageService: MessageService,
    private translate: TranslateService, public dataService: DataService) {
    this.loadRecRules();
    this.refresh();
  }

  refresh() {
    this.refreshing = true;
    this.dvrService.GetUpcomingList({ ShowAll: this.showAllStatuses, RecordId: this.recordId }).subscribe(data => {
      this.programs = data.ProgramList.Programs;
      this.loaded = true;
      this.refreshing = false;
    });
  }

  loadRecRules() {
    this.recRules = [];
    this.errorCount = 0;

    this.dvrService.GetRecordScheduleList({}).subscribe({
      next: (data) => {
        this.recRules = [];
        this.recRules.push(<RecRule>{Id: 0, Title: 'All'});
        data.RecRuleList.RecRules.forEach((entry) => {
          if (entry.Type != 'Recording Template')
            this.recRules.push(entry);
        });
      },
      error: (err: any) => {
        this.errorCount++
      }
    });
  }

  ngOnInit(): void { }

}
