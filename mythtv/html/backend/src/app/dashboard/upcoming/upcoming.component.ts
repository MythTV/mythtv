import { Component, OnInit } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { MessageService } from 'primeng/api';
import { ScheduleLink, SchedulerSummary } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { DvrService } from 'src/app/services/dvr.service';
import { ProgramList, ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';

@Component({
  selector: 'app-upcoming',
  templateUrl: './upcoming.component.html',
  styleUrls: ['./upcoming.component.css'],
  providers: [MessageService]
})
export class UpcomingComponent implements OnInit, SchedulerSummary {


  recordings!: ProgramList;
  editingProgram?: ScheduleOrProgram;
  displayUpdateDlg = false;
  displayUnsaved = false;
  successCount = 0;
  errorCount = 0;
  showAllStatuses = false;
  refreshing = false;
  inter: ScheduleLink = { summaryComponent: this };

  constructor(private dvrService: DvrService, private messageService: MessageService,
    private translate: TranslateService, public dataService: DataService) {
    this.refresh();
  }

  refresh() {
    this.refreshing = true;
    this.dvrService.GetUpcomingList({ ShowAll: this.showAllStatuses }).subscribe(data => {
      this.recordings = data.ProgramList
      this.refreshing = false;
    });
  }

  ngOnInit(): void { }

}
