import { Component, ElementRef, OnInit, QueryList, ViewChild, ViewChildren } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { MessageService, SortMeta } from 'primeng/api';
import { Table, TableLazyLoadEvent } from 'primeng/table';
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

  @ViewChild('table') table!: Table;
  @ViewChildren('row') rows!: QueryList<ElementRef>;

  programs: ScheduleOrProgram[] = [];
  recRules: RuleListEntry[] = [];
  defaultRecRule: RuleListEntry = { Id: 0, Title: 'settings.chanedit.all' };
  recGroups: string[] = [];
  restartErrMsg = 'dashboard.upcoming.restart_err';
  editingProgram?: ScheduleOrProgram;
  displayUpdateDlg = false;
  refreshing = false;
  loaded = false;
  inter: ScheduleLink = { summaryComponent: this };
  lazyLoadEvent?: TableLazyLoadEvent;

  displayStop = false;
  errorCount = 0;
  program?: ScheduleOrProgram;
  totalRecords = 0;
  showTable = false;
  virtualScrollItemSize = 0;
  selectedRule: RuleListEntry | null = null;
  selectedStatus = '';
  selectedRecGroup: string | null = null;
  loadLast = 0;
  sortField = 'StartTime';
  sortOrder = 1;

  constructor(private dvrService: DvrService, private messageService: MessageService,
    private translate: TranslateService, public dataService: DataService,
    private utility: UtilityService) {
    this.translate.get(this.defaultRecRule.Title).subscribe(data => this.defaultRecRule.Title = data);
    this.translate.get(this.restartErrMsg).subscribe(data => this.restartErrMsg = data);

    this.dvrService.GetRecGroupList('schedule')
      .subscribe((data) => {
        this.recGroups = data.RecGroupList;
      });

    let sortField = this.utility.sortStorage.getItem('upcoming.sortField');
    if (sortField)
      this.sortField = sortField;

    let sortOrder = this.utility.sortStorage.getItem('upcoming.sortOrder');
    if (sortOrder)
      this.sortOrder = Number(sortOrder);

    let selectedStatus = sessionStorage.getItem('upcoming.selectedStatus');
    if (selectedStatus)
      this.selectedStatus = selectedStatus;

    this.selectedRecGroup = sessionStorage.getItem('upcoming.selectedRecGroup');
  }

  ngOnInit(): void {
    // Initial Load
    this.loadRecRules();
  }

    onSort(sortMeta: SortMeta) {
    this.sortField = sortMeta.field;
    this.sortOrder = sortMeta.order;
    this.utility.sortStorage.setItem("upcoming.sortField", sortMeta.field);
    this.utility.sortStorage.setItem('upcoming.sortOrder', sortMeta.order.toString());
  }

  fullrefresh() {
    // This is to ensure that if the table get larger the extra lines will show.
    this.showTable = false;
    this.refresh();
  }

  refresh() {
    this.refreshing = true;
    this.loadRecRules();
    this.loadLazy(this.lazyLoadEvent!, true);
  }

  loadRecRules() {
    this.dvrService.GetRecordScheduleList({}).subscribe({
      next: (data) => {
        this.recRules.length = 0;
        this.recRules.push(this.defaultRecRule);
        data.RecRuleList.RecRules.forEach((entry) => {
          if (entry.Type != 'Recording Template') {
            let recRule = {
              Id: entry.Id,
              Title: entry.Title.substring(0, 30) + ' [' + this.utility.recTypeTrans[entry.Type] + ']'
            };
            this.recRules.push(recRule);
          }
        });
        let selectedIdStr = sessionStorage.getItem('upcoming.selectedRule.Id');
        if (selectedIdStr) {
          let selectedId = Number.parseInt(selectedIdStr);
          let selectedRule = this.recRules.find((element) => element.Id == selectedId);
          if (selectedRule) {
            this.selectedRule = selectedRule;
          }
        }
        // initial load
        if (this.programs.length == 0)
          this.loadLazy({ first: 0, rows: 1 });
      },
    });
  }

  loadLazy(event: TableLazyLoadEvent, doRefresh?: boolean) {
    if (event.sortField != this.lazyLoadEvent?.sortField)
      this.loadLast = 0;
    this.lazyLoadEvent = event;
    let request: GetUpcomingRequest = {
      StartIndex: 0,
      Count: 1,
      ShowAll: false
    };
    if (event.first != undefined) {
      request.StartIndex = event.first;
      if (event.last)
        request.Count = event.last - event.first;
      else if (event.rows) {
        request.Count = event.rows;
        event.last = event.first + event.rows;
      }
    }

    if (event.last! > this.loadLast)
      this.loadLast = event.last!;

    if (doRefresh) {
      request.StartIndex = 0;
      request.Count = this.loadLast;
    }

    let sortField = this.sortField;
    if (Array.isArray(event.sortField))
      sortField = event.sortField[0];
    else if (event.sortField)
      sortField = event.sortField;
    if (!sortField)
      sortField = 'StartTime';
    if (sortField == 'Channel.ChanNum')
      request.Sort = 'ChanNum';
    else
      request.Sort = sortField;
    let sortOrder = '';
    if (event.sortOrder && event.sortOrder < 0)
      sortOrder = ' desc';
    request.Sort = request.Sort + sortOrder;

    if (this.selectedStatus == 'All')
      request.ShowAll = true;
    else if (this.selectedStatus && this.selectedStatus != 'Default')
      request.RecStatus = this.selectedStatus;
    if (this.selectedRule != null && this.selectedRule.Id != 0)
      request.RecordId = this.selectedRule.Id;
    if (this.selectedRecGroup)
      request.RecGroup = this.selectedRecGroup

    this.dvrService.GetUpcomingList(request).subscribe(data => {
      let recordings = data.ProgramList;
      this.totalRecords = data.ProgramList.TotalAvailable;
      this.programs.length = this.totalRecords;
      // populate page of virtual programs
      this.programs.splice(recordings.StartIndex, recordings.Programs.length,
        ...recordings.Programs);
      // notify of change
      this.programs = [...this.programs]
      this.refreshing = false;
      this.showTable = true;
      if (!this.virtualScrollItemSize) {
        let row = this.rows.get(0);
        if (row && row.nativeElement.offsetHeight)
          this.virtualScrollItemSize = row.nativeElement.offsetHeight;
        if (this.table) {
          this.table.totalRecords = this.totalRecords;
          this.table.virtualScrollItemSize = this.virtualScrollItemSize;
        }
      }
    });

  }

  onFilter() {
    this.reload();
  }

  reload() {
    sessionStorage.setItem('upcoming.selectedStatus',this.selectedStatus);
    if (this.selectedRule && this.selectedRule.Id != 0)
      sessionStorage.setItem('upcoming.selectedRule.Id',(this.selectedRule.Id).toString());
    else
      sessionStorage.removeItem('upcoming.selectedRule.Id');
    if (this.selectedRecGroup)
      sessionStorage.setItem('upcoming.selectedRecGroup',this.selectedRecGroup);
    else
      sessionStorage.removeItem('upcoming.selectedRecGroup');
    location.reload();
  }

  formatStartDate(program: ScheduleOrProgram, rowIndex: number): string {
    let priorDate = '';
    if (rowIndex > 0 && this.programs[rowIndex - 1]
      && this.programs[rowIndex - 1].Recording.StartTs)
      priorDate = this.utility.formatDate(this.programs[rowIndex - 1].Recording.StartTs, true, true);
    let thisDate = this.utility.formatDate(program.Recording.StartTs, true, true);
    if (priorDate == thisDate)
      return ' ';
    return thisDate;
  }

  formatAirDate(program: ScheduleOrProgram): string {
    if (!program.Airdate)
      return ' ';
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

  override(program: ScheduleOrProgram, newRuleType?: string) {
    if (this.inter.sched) {
      if (program.Recording.RecType == 7 || program.Recording.RecType == 8)
        // If already an override
        this.inter.sched.open(program, undefined, undefined, newRuleType);
      else
        this.inter.sched.open(program, undefined, <RecRule>{ Type: 'Override Recording' }, newRuleType);
    }
  }

  stopRequest(program: ScheduleOrProgram) {
    if (program.Recording.RecordedId) {
      this.program = program;
      this.displayStop = true;
    }
  }

  restartRequest(program: ScheduleOrProgram) {
    if (program.Recording.RecordId) {
      this.program = program;
      this.errorCount = 0;
      this.dvrService.ReactivateRecording({
        // Do not include chanid and starttime because the start time may not
        // match the MythTV database if the recording started early or late
        RecordId: program.Recording.RecordId
      }).subscribe({
        next: (x) => {
          if (x.bool) {
            setTimeout(() => this.inter.summaryComponent.refresh(), 3000);
          }
          else {
            this.errorCount++;
            console.log("Error: dvrService.ReactivateRecording returned false");
            this.sendMessage(this.restartErrMsg);
          }
        },
        error: (err) => {
          this.errorCount++;
          console.log("Error: dvrService.ReactivateRecording returned http error");
          this.sendMessage(this.restartErrMsg);
        }
      });
    }
    else {
      console.log("Error: there is no RecordId for the entry");
      this.sendMessage(this.restartErrMsg);
    }
  }

  sendMessage(text: string) {
    this.messageService.add({
      severity: 'error', summary: this.program?.Title,
      detail: text,
      life: 5000,
      sticky: true
    });
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
