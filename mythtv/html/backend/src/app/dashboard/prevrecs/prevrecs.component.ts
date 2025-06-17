import { Component, ElementRef, OnInit, QueryList, ViewChild, ViewChildren } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { ConfirmationService, MenuItem, MessageService, SortMeta } from 'primeng/api';
import { Menu } from 'primeng/menu';
import { Table, TableLazyLoadEvent } from 'primeng/table';
import { ScheduleLink } from 'src/app/schedule/schedule.component';
import { DvrService } from 'src/app/services/dvr.service';
import { GetOldRecordedListRequest, RemoveOldRecordedRequest, UpdateOldRecordedRequest } from 'src/app/services/interfaces/dvr.interface';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-prevrecs',
  templateUrl: './prevrecs.component.html',
  styleUrls: ['./prevrecs.component.css'],
  providers: [ConfirmationService, MessageService]
})

export class PrevrecsComponent implements OnInit {

  @ViewChild('table') table!: Table;
  @ViewChildren('row') rows!: QueryList<ElementRef>;
  @ViewChild("menu") menu!: Menu;


  programs: ScheduleOrProgram[] = [];
  selection: ScheduleOrProgram[] = [];
  actionList: ScheduleOrProgram[] = [];
  lazyLoadEvent?: TableLazyLoadEvent;

  showTable = false;
  refreshing = false;
  totalRecords = 0;
  virtualScrollItemSize = 0;
  inter: ScheduleLink = { summaryComponent: this };
  searchValue = '';
  subSearchValue = '';
  minDate: Date = new Date();
  minSet = false;
  maxDate: Date = new Date();
  dateValue?: Date | null;
  loadLast = 0;
  sortField = 'StartTime';
  sortOrder = 1;

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    ActionsSelected: 'dashboard.prevrecs.actionsselected',
    DeleteSelected: 'dashboard.prevrecs.deleteselected',
    DeleteWarning: 'dashboard.prevrecs.deletewarning',
    UndefSelection: 'dashboard.undefselection'
  }

  // Menu Items
  mnu_delete: MenuItem = { label: 'dashboard.prevrecs.mnu_delete', command: (event) => this.deleteRequest(event) };
  mnu_rerec: MenuItem = { label: 'dashboard.recordings.mnu_rerec', command: (event) => this.rerec(event, true) };
  mnu_norec: MenuItem = { label: 'dashboard.prevrecs.mnu_norec', command: (event) => this.rerec(event, false) };

  menuToShow: MenuItem[] = [];

  constructor(private dvrService: DvrService, private utility: UtilityService,
    private messageService: MessageService, public translate: TranslateService,
    private confirmationService: ConfirmationService) {

    // translations
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }

    const mnu_entries = [this.mnu_delete, this.mnu_rerec, this.mnu_norec];
    mnu_entries.forEach(entry => {
      if (entry.label)
        this.translate.get(entry.label).subscribe(data =>
          entry.label = data
        );
    });

    let sortField = this.utility.sortStorage.getItem('prevrecs.sortField');
    if (sortField)
      this.sortField = sortField;

    let sortOrder = this.utility.sortStorage.getItem('prevrecs.sortOrder');
    if (sortOrder)
      this.sortOrder = Number(sortOrder);
  }

  ngOnInit(): void {
    // Initial Load
    this.loadLazy({ first: 0, rows: 1 });
  }

  onSort(sortMeta: SortMeta) {
    this.sortField = sortMeta.field;
    this.sortOrder = sortMeta.order;
    this.utility.sortStorage.setItem("prevrecs.sortField", sortMeta.field);
    this.utility.sortStorage.setItem('prevrecs.sortOrder', sortMeta.order.toString());
  }


  reload() {
    this.showTable = false;
    this.programs.length = 0;
    this.totalRecords = 0;
    this.refreshing = true;
    this.lazyLoadEvent = undefined;
    this.loadLast = 0;
    this.loadLazy(({ first: 0, rows: 1 }));
  }

  refresh() {
    this.selection = [];
    this.menu.hide();
    this.refreshing = true;
    if (this.lazyLoadEvent!.last! > 15)
      this.loadLazy(this.lazyLoadEvent!, true);
    else
    // If there are only a few rows do a full reload because the size
    // may have changed and the loadlazy will not increase the size.
      this.reload();
  }

  loadLazy(event: TableLazyLoadEvent, doRefresh?: boolean) {
    if (event.sortField != this.lazyLoadEvent?.sortField)
      this.loadLast = 0;
    this.lazyLoadEvent = event;
    let request: GetOldRecordedListRequest = {
      StartIndex: 0,
      Count: 1
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

    if (this.dateValue) {
      let startTime = this.dateValue;
      // Add 1 second to start time so that programs that
      // end at midnight prior month are not included.
      startTime.setTime(startTime.getTime() + 1000);
      request.StartTime = startTime.toISOString();
      let endTime = new Date(this.dateValue);
      endTime.setMonth(endTime.getMonth() + 1);
      // Subtract 1 second from end time so that programs that
      // start at 12 AM next month are not included.
      endTime.setTime(endTime.getTime() - 1000);
      request.EndTime = endTime.toISOString();
    }
    request.TitleRegex = this.searchValue;
    request.SubtitleRegex = this.subSearchValue;

    let sortField = this.sortField;
    if (Array.isArray(event.sortField))
      sortField = event.sortField[0];
    else if (event.sortField)
      sortField = event.sortField;
    request.Sort = sortField;
    if (event.sortOrder && event.sortOrder < 0)
      request.Descending = true;

    this.dvrService.GetOldRecordedList(request).subscribe(data => {
      let recordings = data.ProgramList;
      if (data.ProgramList.TotalAvailable) {
        this.totalRecords = data.ProgramList.TotalAvailable;
        this.programs.length = this.totalRecords;
      }
      if (!this.minSet) {
        this.minDate = new Date(recordings.Programs[0].StartTime);
        this.minSet = true;
      }
      // populate page of virtual programs
      if (recordings.Count == 0 && recordings.StartIndex == 0)
        this.programs = [];
      else
        this.programs.splice(recordings.StartIndex, recordings.Count,
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

  showContextMenu(program: ScheduleOrProgram | null, event: any) {
    this.actionList.length = 0;
    if (program && program.Title)
      this.actionList.push(program);
    else
      this.actionList.push(...this.selection);
    if (this.actionList.length == 0)
      return;
    if (this.actionList.some((x) => !x)) {
      this.sendMessage('error', null, '', this.msg.UndefSelection);
      return;
    }
    this.menuToShow.length = 0;
    let subMenu: MenuItem[] = [];
    if (this.actionList.some((x) => x.ProgramFlagNames.indexOf('DUPLICATE') >= 0))
      subMenu.push(this.mnu_rerec);
    if (this.actionList.some((x) => x.ProgramFlagNames.indexOf('DUPLICATE') < 0))
      subMenu.push(this.mnu_norec);
    subMenu.push(this.mnu_delete);
    if (this.actionList.length == 1) {
      this.menuToShow.push({ label: this.actionList[0].Title + ' - ' + this.actionList[0].SubTitle, items: subMenu });
    }
    else
      this.menuToShow.push({ label: this.msg.ActionsSelected.replace(/{{ *num *}}/, this.actionList.length.toString()), items: subMenu });

    // Notify Angular that menu has changed
    this.menuToShow = [...this.menuToShow];
    this.menu.toggle(event);
  }

  resetSearch() {
    this.searchValue = '';
    this.reload();
  }

  resetSubSearch() {
    this.subSearchValue = '';
    this.reload();
  }

  onFilter() {
    this.reload();
  }
  onSelectChange() {
    this.menu.hide();
  }

  // return true causes default browser right click menu to show
  // return false suppresses default browser right click menu
  onContextMenu(program: ScheduleOrProgram, event: any) {
    if (this.selection.length == 0)
      return true;
    if (this.selection.some((x) => !x)) {
      // This happens if some entries have not been loaded
      this.sendMessage('error', null, '', this.msg.UndefSelection);
      return false;
    }
    if (this.selection.some((x) => x.Recording.RecordedId == program.Recording.RecordedId)) {
      this.showContextMenu(null, event);
      return false;
    }
    return true;
  }

  sendMessage(severity: string, program: ScheduleOrProgram | null, action: string, text: string, extraText?: string) {
    if (extraText)
      extraText = '\n' + extraText;
    else
      extraText = '';
    let detail = action;
    if (program != null)
      detail = action + ' ' + program.Title + ' ' + program.SubTitle + extraText;
    this.messageService.add({
      severity: severity, summary: text,
      detail: detail,
      life: 5000,
      sticky: (severity == 'error')
    });
  }

  keydown(event: KeyboardEvent) {
    if (event.key == "Enter")
      this.onFilter();
  }

  deleteRequest(event: any) {
    let header: string;
    if (this.actionList.length > 1)
      header = this.msg.DeleteSelected.replace(/{{ *num *}}/, this.actionList.length.toString());
    else if (this.actionList.length == 1) {
      let program = this.actionList[0];
      header = program.Title + ': ' + program.SubTitle;
    }
    else
      return;

    this.confirmationService.confirm({
      message: this.msg.DeleteWarning,
      header: header,
      icon: 'pi pi-exclamation-triangle',
      accept: () => {
        this.delete(event);
      },
    });

  }

  delete(event: any) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      let param: RemoveOldRecordedRequest = {
        Chanid: program.Channel.ChanId,
        StartTime: new Date(program.StartTime),
        Reschedule: this.actionList.length == 0
      };
      this.dvrService.RemoveOldRecorded(param).subscribe({
        next: (x) => {
          if (x.bool)
            this.sendMessage('success', program, event.item.label, this.msg.Success);
          else
            this.sendMessage('error', program, event.item.label, this.msg.Failed);
          this.delete(event);
        },
        error: (err: any) => {
          this.networkError(program, err);
          this.delete(event);
        }
      });
    }
    else {
      setTimeout(() => this.refresh(), 1000);
    }
  }

  rerec(event: any, enable: boolean) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      let param: UpdateOldRecordedRequest = {
        Chanid: program.Channel.ChanId,
        StartTime: new Date(program.StartTime),
        Duplicate: !enable,
        Reschedule: this.actionList.length == 0
      };
      this.dvrService.UpdateOldRecorded(param).subscribe({
        next: (x) => {
          if (x.bool)
            this.sendMessage('success', program, event.item.label, this.msg.Success);
          else
            this.sendMessage('error', program, event.item.label, this.msg.Failed);
          this.rerec(event, enable);
        },
        error: (err: any) => {
          this.networkError(program, err);
          this.rerec(event, enable);
        }
      });
    }
    else {
      setTimeout(() => this.refresh(), 1000);
    }
  }

  networkError(program: ScheduleOrProgram, err: any) {
    console.log("network error", err);
    this.sendMessage('error', program, '', this.msg.NetFail);
  }

}
