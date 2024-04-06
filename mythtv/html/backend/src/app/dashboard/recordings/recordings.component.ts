import { Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { FilterMatchMode, LazyLoadEvent, MenuItem, MessageService, SelectItem } from 'primeng/api';
import { Menu } from 'primeng/menu';
import { PartialObserver } from 'rxjs';
import { DvrService } from 'src/app/services/dvr.service';
import { GetRecordedListRequest, UpdateRecordedMetadataRequest } from 'src/app/services/interfaces/dvr.interface';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { JobQCommands } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-recordings',
  templateUrl: './recordings.component.html',
  styleUrls: ['./recordings.component.css'],
  providers: [MessageService]
})
export class RecordingsComponent implements OnInit {

  @ViewChild("recsform") currentForm!: NgForm;
  @ViewChild("menu") menu!: Menu;

  programs: ScheduleOrProgram[] = [];
  selection: ScheduleOrProgram[] = [];
  actionList: ScheduleOrProgram[] = [];
  recGroups: string[] = [];
  newRecGroup = '';
  lazyLoadEvent!: LazyLoadEvent;
  JobQCmds!: JobQCommands;
  program: ScheduleOrProgram = <ScheduleOrProgram>{ Title: '', Recording: {} };
  editingProgram?: ScheduleOrProgram;
  displayMetadataDlg = false;
  displayRecGrpDlg = false;
  displayRunJobs = false;
  displayUnsaved = false;
  successCount = 0;
  errorCount = 0;
  refreshing = false;
  priorRequest: GetRecordedListRequest = {};

  msg = {
    Success: 'common.success',
    Failed: 'common.failed',
    NetFail: 'common.networkfail',
    CanUndo: 'dashboard.recordings.canundel',
    AlreadyDel: 'dashboard.recordings.alreadydel',
    NonReRec: 'dashboard.recordings.nonrerec',
    ActionsSelected: 'dashboard.recordings.actionsselected',
    JobsSelected: 'dashboard.recordings.jobsselected',
    UndefSelection: 'dashboard.undefselection'
  }

  jobsoffset = 3; // number of items before user jobs
  jobs: MenuItem[] = [
    { id: 'Transcode', label: 'dashboard.recordings.job_Transcode', command: (event) => this.runjob(event) },
    { id: 'Commflag', label: 'dashboard.recordings.job_Commflag', command: (event) => this.runjob(event) },
    { id: 'Metadata', label: 'dashboard.recordings.job_Metadata', command: (event) => this.runjob(event) },
    { id: 'UserJob1', visible: false, command: (event) => this.runjob(event) },
    { id: 'UserJob2', visible: false, command: (event) => this.runjob(event) },
    { id: 'UserJob3', visible: false, command: (event) => this.runjob(event) },
    { id: 'UserJob4', visible: false, command: (event) => this.runjob(event) }
  ];

  // Menu Items
  mnu_delete: MenuItem = { label: 'dashboard.recordings.mnu_delete', command: (event) => this.delete(event, false) };
  mnu_delete_rerec: MenuItem = { label: 'dashboard.recordings.mnu_delete_rerec', command: (event) => this.delete(event, true) };
  mnu_undelete: MenuItem = { label: 'dashboard.recordings.mnu_undelete', command: (event) => this.undelete(event) };
  mnu_rerec: MenuItem = { label: 'dashboard.recordings.mnu_rerec', command: (event) => this.rerec(event) };
  mnu_markwatched: MenuItem = { label: 'dashboard.recordings.mnu_markwatched', command: (event) => this.markwatched(event, true) };
  mnu_markunwatched: MenuItem = { label: 'dashboard.recordings.mnu_markunwatched', command: (event) => this.markwatched(event, false) };
  mnu_markdamaged: MenuItem = { label: 'dashboard.recordings.mnu_markdamaged', command: (event) => this.markdamaged(event, true) };
  mnu_markundamaged: MenuItem = { label: 'dashboard.recordings.mnu_markundamaged', command: (event) => this.markdamaged(event, false) };
  mnu_updaterecgrp: MenuItem = { label: 'dashboard.recordings.mnu_updaterecgrp', command: (event) => this.promptrecgrp(event) };
  mnu_updatemeta: MenuItem = { label: 'dashboard.recordings.mnu_updatemeta', command: (event) => this.updatemeta(event) };
  mnu_updaterecrule: MenuItem = { label: 'dashboard.recordings.mnu_updaterecrule', command: (event) => this.updaterecrule(event) };
  mnu_stoprec: MenuItem = { label: 'dashboard.recordings.mnu_stoprec', command: (event) => this.stoprec(event) };
  mnu_runjobs: MenuItem = { label: 'dashboard.recordings.mnu_runjobs', items: this.jobs };

  menuToShow: MenuItem[] = [];

  matchModeRecGrp: SelectItem[] = [{
    value: FilterMatchMode.EQUALS,
    label: 'common.filter.equals'
  }];

  matchModeTitle: SelectItem[] = [{
    value: FilterMatchMode.STARTS_WITH,
    label: 'common.filter.startswith',
  },
  {
    value: FilterMatchMode.CONTAINS,
    label: 'common.filter.contains',
  },
  {
    value: FilterMatchMode.EQUALS,
    label: 'common.filter.equals',
  }
  ];

  constructor(private dvrService: DvrService, private messageService: MessageService,
    public translate: TranslateService, private setupService: SetupService,
    public utility: UtilityService) {
    this.JobQCmds = this.setupService.getJobQCommands();

    this.dvrService.GetRecGroupList()
      .subscribe((data) => {
        this.recGroups = data.RecGroupList;
        this.recGroups.push('Deleted');
      });
    // translations
    for (const [key, value] of Object.entries(this.msg)) {
      this.translate.get(value).subscribe(data => {
        Object.defineProperty(this.msg, key, { value: data });
      });
    }

    const mnu_entries = [this.mnu_delete, this.mnu_delete_rerec, this.mnu_undelete, this.mnu_rerec, this.mnu_markwatched,
    this.mnu_markunwatched, this.mnu_markdamaged, this.mnu_markundamaged, this.mnu_updatemeta, this.mnu_updaterecrule,
    this.mnu_stoprec, this.mnu_updaterecgrp, this.mnu_runjobs, this.jobs[0], this.jobs[1], this.jobs[2],
    ...this.matchModeRecGrp, ...this.matchModeTitle]

    mnu_entries.forEach(entry => {
      if (entry.label)
        this.translate.get(entry.label).subscribe(data =>
          entry.label = data
        );
    });
  }

  ngOnInit(): void {
  }

  loadLazy(event: LazyLoadEvent) {
    this.lazyLoadEvent = event;
    let request: GetRecordedListRequest = {
      StartIndex: 0,
      Count: 1
    };
    if (event.first)
      request.StartIndex = event.first;
    if (event.rows)
      request.Count = event.rows;
    if (!event.sortField)
      event.sortField = 'Title';
    request.Sort = event.sortField
    if (event.sortField == 'Airdate')
      request.Sort = 'originalairdate';
    else if (event.sortField == 'Recording.RecGroup')
      request.Sort = 'recgroup';
    else
      request.Sort = event.sortField;
    let sortOrder = ' asc';
    if (event.sortOrder && event.sortOrder < 0)
      sortOrder = ' desc';
    request.Sort = request.Sort + sortOrder;
    request.Sort += `,title${sortOrder},originalairdate${sortOrder},season${sortOrder},episode${sortOrder}`;
    if (event.filters) {
      if (event.filters.Title.value) {
        switch (event.filters.Title.matchMode) {
          case FilterMatchMode.STARTS_WITH:
            request.TitleRegEx = '^' + event.filters.Title.value;
            break;
          case FilterMatchMode.CONTAINS:
            request.TitleRegEx = event.filters.Title.value;
            break;
          case FilterMatchMode.EQUALS:
            request.TitleRegEx = '^' + event.filters.Title.value + '$';
            break;
        }
      }
      if (event.filters['Recording.RecGroup'].value) {
        if (event.filters['Recording.RecGroup'].matchMode == FilterMatchMode.EQUALS)
          request.RecGroup = event.filters['Recording.RecGroup'].value;
      }
    }
    if (request.TitleRegEx != this.priorRequest.TitleRegEx
      || request.RecGroup != this.priorRequest.RecGroup) {
      this.programs = [];
      this.selection = [];
      this.menu.hide();
      this.priorRequest = request;
    }

    this.dvrService.GetRecordedList(request).subscribe(data => {
      let recordings = data.ProgramList;
      this.programs.length = data.ProgramList.TotalAvailable;
      // populate page of virtual programs
      this.programs.splice(recordings.StartIndex, recordings.Count,
        ...recordings.Programs);
      // notify of change
      this.programs = [...this.programs]
      this.refreshing = false;
    });
  }

  refresh() {
    this.selection = [];
    this.menu.hide();
    this.loadLazy(this.lazyLoadEvent);
  }

  URLencode(x: string): string {
    return encodeURI(x);
  }

  getDuration(program: ScheduleOrProgram): number {
    const starttm = new Date(program.Recording.StartTs).getTime();
    const endtm = new Date(program.Recording.EndTs).getTime();
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  // return true causes default browser right click menu to show
  // return false suppresses default browser right click menu
  onContextMenu(program: ScheduleOrProgram, event: any) {
    if (this.selection.length == 0)
      return true;
    if (event.target && event.target.id && event.target.id.startsWith('download_'))
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

  onSelectChange() {
    this.menu.hide();
  }

  // todo: add a way of showing and updating these:
  // AutoExpire, Preserve
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
    if (this.actionList.some((x) => x.Recording.RecGroup == 'Deleted'))
      subMenu.push(this.mnu_undelete);
    if (this.actionList.some((x) => x.Recording.RecGroup != 'Deleted')) {
      subMenu.push(this.mnu_delete);
      subMenu.push(this.mnu_delete_rerec);
    }
    subMenu.push(this.mnu_rerec);
    if (this.actionList.some((x) => x.ProgramFlagNames.indexOf('WATCHED') > -1))
      subMenu.push(this.mnu_markunwatched);
    if (this.actionList.some((x) => x.ProgramFlagNames.indexOf('WATCHED') < 0))
      subMenu.push(this.mnu_markwatched);
    if (this.actionList.some((x) => x.VideoPropNames.indexOf('DAMAGED') > -1))
      subMenu.push(this.mnu_markundamaged);
    if (this.actionList.some((x) => x.VideoPropNames.indexOf('DAMAGED') < 0))
      subMenu.push(this.mnu_markdamaged);
    subMenu.push(this.mnu_updaterecgrp);
    if (this.actionList.length == 1) {
      subMenu.push(this.mnu_updatemeta);
      this.menuToShow.push({ label: this.actionList[0].Title + ' - ' + this.actionList[0].SubTitle, items: subMenu });
    }
    else
      this.menuToShow.push({ label: this.msg.ActionsSelected.replace(/{{ *num *}}/, this.actionList.length.toString()), items: subMenu });
    if (this.actionList.every((x) => x.Recording.RecGroup != 'Deleted')) {
      if (this.actionList.length == 1)
        this.menuToShow.push(this.mnu_runjobs)
      else
        this.menuToShow.push({ label: this.msg.JobsSelected.replace(/{{ *num *}}/, this.actionList.length.toString()), items: this.jobs });
      for (let ix = 0; ix < 4; ix++) {
        if (this.JobQCmds.UserJob[ix]) {
          this.jobs[ix + this.jobsoffset].visible = true;
          this.jobs[ix + this.jobsoffset].label = this.JobQCmds.UserJobDesc[ix];
        }
        else
          this.jobs[ix + this.jobsoffset].visible = false;
      }
    }
    // todo: implement this
    // this.menuToShow.push(this.mnu_updaterecrule);
    this.menu.toggle(event);
  }

  delete(event: any, rerec: boolean) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      // check with backend not already deleted by another user
      this.dvrService.GetRecorded({ RecordedId: program.Recording.RecordedId })
        .subscribe({
          next: (x) => {
            if (x.Program.Recording.RecGroup == 'Deleted') {
              this.sendMessage('error', program, event.item.label, this.msg.AlreadyDel);
              program.Recording.RecGroup = 'Deleted';
            }
            else {
              this.dvrService.DeleteRecording({
                RecordedId: program.Recording.RecordedId,
                AllowRerecord: rerec
              }).subscribe({
                next: (x) => {
                  if (x.bool) {
                    this.sendMessage('success', program, event.item.label, this.msg.Success, this.msg.CanUndo);
                    program.Recording.RecGroup = 'Deleted';
                  }
                  else {
                    this.sendMessage('error', program, event.item.label, this.msg.Failed);
                  }
                },
                error: (err: any) => this.networkError(program, err)
              });
            }
            this.delete(event, rerec);
          },
          error: (err: any) => {
            this.networkError(program, err);
            this.delete(event, rerec);
          }
        });
    }
  }

  undelete(event: any) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      this.dvrService.UnDeleteRecording({ RecordedId: program.Recording.RecordedId, }).subscribe({
        next: (x) => {
          if (x.bool) {
            program.Recording.RecGroup = 'Default';
            this.sendMessage('success', program, event.item.label, this.msg.Success);
          }
          else {
            this.sendMessage('error', program, event.item.label, this.msg.Failed);
          }
          this.undelete(event);
        },
        error: (err: any) => {
          this.networkError(program, err);
          this.undelete(event);
        }
      });
    }
  }

  networkError(program: ScheduleOrProgram, err: any) {
    console.log("network error", err);
    this.sendMessage('error', program, '', this.msg.NetFail);
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

  rerec(event: any) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      this.dvrService.AllowReRecord(program.Recording.RecordedId).subscribe({
        next: (x) => {
          if (x.bool)
            this.sendMessage('success', program, event.item.label, this.msg.Success);
          else
            this.sendMessage('error', program, event.item.label, this.msg.Failed);
          this.rerec(event);
        },
        error: (err: any) => {
          this.networkError(program, err);
          this.rerec(event);
        }
      });
    }
  }
  markwatched(event: any, Watched: boolean) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      this.dvrService.UpdateRecordedMetadata(
        { RecordedId: program.Recording.RecordedId, Watched: Watched }).subscribe({
          next: (x) => {
            if (x.bool) {
              this.sendMessage('success', program, event.item.label, this.msg.Success);
              if (Watched) {
                program.ProgramFlagNames = program.ProgramFlagNames + '|WATCHED|';
              }
              else {
                const regex = /WATCHED/g;
                program.ProgramFlagNames = program.ProgramFlagNames.replace(regex, '');
              }
            }
            else
              this.sendMessage('error', program, event.item.label, this.msg.Failed);
            this.markwatched(event, Watched);
          },
          error: (err: any) => {
            this.networkError(program, err);
            this.markwatched(event, Watched);
          }
        });
    }

  }

  markdamaged(event: any, damaged: boolean) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      this.dvrService.UpdateRecordedMetadata(
        { RecordedId: program.Recording.RecordedId, Damaged: damaged }).subscribe({
          next: (x) => {
            if (x.bool) {
              if (damaged) {
                this.sendMessage('success', program, event.item.label, this.msg.Success, this.msg.NonReRec);
                program.VideoPropNames = program.VideoPropNames + '|DAMAGED|';
              }
              else {
                this.sendMessage('success', program, event.item.label, this.msg.Success);
                const regex = /DAMAGED/g;
                program.VideoPropNames = program.VideoPropNames.replace(regex, '');
              }
            }
            else
              this.sendMessage('error', program, event.item.label, this.msg.Failed);
            this.markdamaged(event, damaged);
          },
          error: (err: any) => {
            this.networkError(program, err);
            this.markdamaged(event, damaged);
          }
        });
    }
  }

  promptrecgrp(event: any) {
    if (this.actionList.length == 1)
      this.newRecGroup = this.actionList[0].Recording.RecGroup;
    else
      this.newRecGroup = '';
    this.displayRecGrpDlg = true;
  }


  updaterecgrp() {
    this.displayRecGrpDlg = false;
    this.newRecGroup = this.newRecGroup.trim();
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program && this.newRecGroup) {
      this.dvrService.UpdateRecordedMetadata(
        { RecordedId: program.Recording.RecordedId, RecGroup: this.newRecGroup }).subscribe({
          next: (x) => {
            if (x.bool) {
              this.sendMessage('success', program, <string>this.mnu_updaterecgrp.label,
                this.msg.Success);
              program.Recording.RecGroup = this.newRecGroup;
            }
            else
              this.sendMessage('error', program, <string>this.mnu_updaterecgrp.label,
                this.msg.Failed);
            this.updaterecgrp();
          },
          error: (err: any) => {
            this.networkError(program, err);
            this.updaterecgrp();
          }
        });
    }
  }

  updaterecrule(event: any) { }
  stoprec(event: any) { }

  runjob(event: any) {
    let program = <ScheduleOrProgram>this.actionList.shift();
    if (program) {
      this.dvrService.ManageJobQueue({
        Action: 'Add',
        JobName: event.item.id,
        RecordedId: program.Recording.RecordedId,
      }).subscribe({
        next: (x) => {
          if (x.int > 0) {
            this.sendMessage('success', program, event.item.label, this.msg.Success);
          }
          else
            this.sendMessage('error', program, event.item.label, this.msg.Failed);
          this.runjob(event);
        },
        error: (err: any) => {
          this.networkError(program, err);
          this.runjob(event);
        }
      });
    }
  }


  updatemeta(event: any) {
    this.program = <ScheduleOrProgram>this.actionList.shift();
    if (this.program) {
      this.editingProgram = this.program;
      this.program = Object.assign({}, this.program);
      if (this.program.Airdate)
        this.program.Airdate = new Date(this.program.Airdate + ' 00:00');
      else
        this.program.Airdate = <Date><unknown>null;
      this.displayMetadataDlg = true;
      this.currentForm.form.markAsPristine();
    }
  }

  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        this.successCount++;
        this.currentForm.form.markAsPristine();
        if (this.editingProgram)
          Object.assign(this.editingProgram, this.program);
      }
      else {
        console.log("saveObserver error", x);
        this.errorCount++;
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.errorCount++;
    }
  };

  saveProgram() {
    this.successCount = 0;
    this.errorCount = 0;
    this.displayUnsaved = false;
    let request: UpdateRecordedMetadataRequest = {
      RecordedId: this.program.Recording.RecordedId,
      Description: this.program.Description,
      Episode: this.program.Episode,
      Inetref: this.program.Inetref,
      OriginalAirDate: this.program.Airdate,
      Season: this.program.Season,
      SubTitle: this.program.SubTitle,
      Title: this.program.Title,
      RecGroup: this.program.Recording.RecGroup
    };

    this.dvrService.UpdateRecordedMetadata(request).subscribe(this.saveObserver);

  }

  closeDialog() {
    if (this.currentForm.dirty) {
      if (this.displayUnsaved) {
        // Close on the unsaved warning
        this.displayUnsaved = false;
        this.displayMetadataDlg = false;
        this.editingProgram = undefined;
        this.currentForm.form.markAsPristine();
      }
      else
        // Close on the channel edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
    }
    else {
      // Close on the channel edit dialog
      this.displayMetadataDlg = false;
      this.displayUnsaved = false;
      this.editingProgram = undefined;
    };

  }


}
