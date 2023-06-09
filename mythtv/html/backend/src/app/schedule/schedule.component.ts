import { Component, HostListener, Input, OnInit, ViewChild } from '@angular/core';
import { ScheduleOrProgram } from '../services/interfaces/program.interface';
import { RecRule, RecRuleFilter } from '../services/interfaces/recording.interface';
import { DvrService } from '../services/dvr.service';
import { InputList } from '../services/interfaces/input.interface';
import { Channel } from '../services/interfaces/channel.interface';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from '../services/myth.service';
import { NgForm } from '@angular/forms';
import { RecordScheduleRequest } from '../services/interfaces/dvr.interface';
import { Observable, of } from 'rxjs';

export interface SchedulerSummary {
  refresh(): void;
}

export interface ScheduleLink {
  sched?: ScheduleComponent;
  summaryComponent: SchedulerSummary;
}

interface ListEntry {
  prompt: string;
  value: string;
}

interface BoolAssociativeArray {
  [key: string]: boolean
}


@Component({
  selector: 'app-schedule',
  templateUrl: './schedule.component.html',
  styleUrls: ['./schedule.component.css']
})
export class ScheduleComponent implements OnInit {
  @Input() inter!: ScheduleLink;
  @ViewChild("schedform") currentForm!: NgForm;

  displayDlg = false;
  displayUnsaved = false;
  loadCount = 0;
  successCount = 0;
  errorCount = 0;
  program?: ScheduleOrProgram;
  channel?: Channel;
  recRule?: RecRule;
  defaultTemplate?: RecRule;
  reqProgram?: ScheduleOrProgram
  reqChannel?: Channel
  reqRecRule?: RecRule

  recRules: RecRule[] = [];
  playGroups: string[] = [];
  recGroups: string[] = [];
  recStorageGroups: string[] = [];
  inputList: InputList = {
    Inputs: []
  };
  recRuleFilters: RecRuleFilter[] = [];
  selectedFilters: number[] = [];
  templates: RecRule[] = [];
  typeList: ListEntry[] = [];

  dupMethodList: ListEntry[] = [
    { prompt: this.translate.instant('dashboard.sched.dupmethod.none'), value: 'None' },
    { prompt: this.translate.instant('dashboard.sched.dupmethod.s_and_d'), value: 'Subtitle and Description' },
    { prompt: this.translate.instant('dashboard.sched.dupmethod.s_then_d'), value: 'Subtitle then Description' },
    { prompt: this.translate.instant('dashboard.sched.dupmethod.s'), value: 'Subtitle' },
    { prompt: this.translate.instant('dashboard.sched.dupmethod.d'), value: 'Description' },
  ];

  dupInList: ListEntry[] = [
    { prompt: this.translate.instant('dashboard.sched.dupin.both'), value: 'All Recordings' },
    { prompt: this.translate.instant('dashboard.sched.dupin.curr'), value: 'Current Recordings' },
    { prompt: this.translate.instant('dashboard.sched.dupin.prev'), value: 'Previous Recordings' },
  ];

  recProfileList: ListEntry[] = [
    { prompt: this.translate.instant('dashboard.sched.recprof.default'), value: 'Default' },
    { prompt: this.translate.instant('dashboard.sched.recprof.livetv'), value: 'Live TV' },
    { prompt: this.translate.instant('dashboard.sched.recprof.highq'), value: 'High Quality' },
    { prompt: this.translate.instant('dashboard.sched.recprof.lowq'), value: 'Low Quality' },
  ];

  postProcList: ListEntry[] = [
    { prompt: this.translate.instant('dashboard.sched.postproc.autocommflag'), value: 'AutoCommflag' },
    { prompt: this.translate.instant('dashboard.sched.postproc.autometalookup'), value: 'AutoMetaLookup' },
    { prompt: this.translate.instant('dashboard.sched.postproc.autotranscode'), value: 'AutoTranscode' },
    { prompt: '1', value: 'AutoUserJob1' },
    { prompt: '2', value: 'AutoUserJob2' },
    { prompt: '3', value: 'AutoUserJob3' },
    { prompt: '4', value: 'AutoUserJob4' },
  ];

  selectedPostProc: string[] = [];

  metaPrefix = '';
  templateId = 0;

  constructor(private dvrService: DvrService, private translate: TranslateService,
    public mythService: MythService) {
  }

  ngOnInit(): void {
    this.inter.sched = this;
  }

  loadLists() {
    this.recRules = [];
    this.playGroups = [];
    this.recGroups = [];
    this.recStorageGroups = [];
    this.inputList.Inputs = [];
    this.recRuleFilters = [];
    this.loadCount = 0;
    this.errorCount = 0;

    this.dvrService.GetRecordScheduleList({}).subscribe({
      next: (data) => {
        this.recRules = data.RecRuleList.RecRules;
        this.loadSuccess();
      },
      error: (err: any) => this.loadFail()
    });
    this.dvrService.GetPlayGroupList().subscribe({
      next: (data) => {
        this.playGroups = data.PlayGroupList;
        this.playGroups.unshift('Default');
        this.loadSuccess();
      },
      error: (err: any) => this.loadFail()
    });
    this.dvrService.GetRecGroupList().subscribe({
      next: (data) => {
        this.recGroups = data.RecGroupList;
        this.loadSuccess();
      },
      error: (err: any) => this.loadFail()
    });
    this.dvrService.GetRecStorageGroupList().subscribe({
      next: (data) => {
        this.recStorageGroups = data.RecStorageGroupList;
        this.loadSuccess();
      },
      error: (err: any) => this.loadFail()
    });
    this.dvrService.GetInputList().subscribe({
      next: (data) => {
        this.inputList = data.InputList;
        this.inputList.Inputs.unshift({
          Id: 0,
          CardId: 0,
          SourceId: 0,
          InputName: '',
          DisplayName: this.translate.instant('dashboard.sched.input_any'),
          QuickTune: false,
          RecPriority: 0,
          ScheduleOrder: 0,
          LiveTVOrder: 0
        });
        this.loadSuccess();
      },
      error: (err: any) => this.loadFail()
    });
    this.dvrService.GetRecRuleFilterList().subscribe({
      next: (data) => {
        this.recRuleFilters = data.RecRuleFilterList.RecRuleFilters;
        this.loadSuccess();
      },
      error: (err: any) => this.loadFail()
    });

    let defaultName;
    for (let num = 1; num < 5; num++) {
      defaultName = this.translate.instant('settings.services.job_default', { num: num });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJobDesc" + num, Default: defaultName })
        .subscribe({
          next: data => { this.postProcList[num + 2].prompt = data.String; this.loadSuccess(); },
          error: () => this.loadFail()
        });
    }
  }

  loadSuccess() {
    this.loadCount++;
    if (this.loadCount == 10) {
      this.loadCount = 0;
      this.setupData();
      this.displayDlg = true;
      this.currentForm.form.markAsPristine();
    }
  }

  loadFail() {
    this.errorCount++;
  }

  open(program?: ScheduleOrProgram, channel?: Channel, recRule?: RecRule) {
    this.reqProgram = program;
    this.reqChannel = channel;
    this.reqRecRule = recRule;
    this.loadLists();
  }

  setupData() {
    this.program = this.reqProgram;
    if (this.reqRecRule)
      this.recRule = Object.assign({}, this.reqRecRule);
    else
      this.recRule = undefined;
    this.channel = this.reqChannel;
    if (this.program) {
      if (!this.channel && this.program.Channel)
        this.channel = this.program.Channel;
    }

    var recId = 0;
    this.typeList = [];
    let ruleType: string = '';
    if (this.reqRecRule)
      ruleType = this.reqRecRule.Type;
    this.templates = [<RecRule>{ Id: 0, Title: '' }];
    this.defaultTemplate = undefined;
    if (this.program && this.program.Recording)
      recId = this.program.Recording.RecordId;
    this.recRules.forEach((entry, index) => {
      if (entry.Id == recId) {
        this.recRule = entry;
        ruleType = this.recRule.Type;
      }
      if (entry.Type == 'Recording Template') {
        this.templates.push(entry);
        if (entry.Title == 'Default (Template)')
          this.defaultTemplate = entry;
      }
    });
    if (!this.recRule) {
      this.recRule = <RecRule>{ Id: 0 };
      if (this.defaultTemplate)
        this.mergeTemplate(this.recRule, this.defaultTemplate)
      ruleType = 'Not Recording';
    }
    if (!this.recRule.SearchType)
      this.recRule.SearchType = 'None';

    // if (this.program && this.channel && this.recRule.Id == 0)
    if (this.program && this.channel && this.recRule.SearchType == 'None')
      this.mergeProgram(this.recRule, this.program, this.channel);

    if (!ruleType)
      ruleType = 'Not Recording';
    if (!this.recRule.StartTime)
      this.recRule.StartTime = (new Date()).toISOString();

    this.filterFromRec(this.recRule);
    this.postProcFromRec(this.recRule);

    if (ruleType == 'Recording Template') {
      this.typeList.push(
        {
          prompt: this.translate.instant('dashboard.sched.type.mod_template'),
          value: 'Recording Template'
        }
      );
      if (this.recRule.Category != 'Default') {
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.del_template'),
            value: 'Not Recording'
          }
        );
      }
    }
    else if (ruleType == 'Override Recording')
      this.typeList.push(
        {
          prompt: this.translate.instant('dashboard.sched.type.del_override'),
          value: 'Not Recording'
        },
        {
          prompt: this.translate.instant('dashboard.sched.type.rec_override'),
          value: 'Override Recording'
        },
        {
          prompt: this.translate.instant('dashboard.sched.type.dont_rec_override'),
          value: 'Do not Record'
        }
      );
    else {
      const isManual = (this.recRule.SearchType == 'Manual Search');
      const isSearch = (this.recRule.SearchType != 'None');
      this.typeList.push(
        {
          prompt: this.translate.instant('dashboard.sched.type.not'),
          value: 'Not Recording'
        }
      );
      if (this.recRule.CallSign && !isSearch)
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.this'),
            value: 'Single Record'
          }
        );
      if (!isManual)
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.one'),
            value: 'Record One'
          }
        );
      if (!this.recRule.CallSign || isSearch)
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.weekly'),
            value: 'Record Weekly'
          },
          {
            prompt: this.translate.instant('dashboard.sched.type.daily'),
            value: 'Record Daily'
          },
        );
      if (!isManual)
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.all'),
            value: 'Record All'
          }
        );
    }
    setTimeout(() => {
      if (this.recRule)
        this.recRule.Type = ruleType;
      this.currentForm.form.markAsPristine();
    }, 10);
  }


  mergeProgram(recRule: RecRule, program: ScheduleOrProgram, channel: Channel) {
    recRule.Title = program.Title;
    recRule.SubTitle = program.SubTitle;
    recRule.Description = program.Description;
    recRule.Category = program.Category;
    recRule.StartTime = program.StartTime;
    recRule.EndTime = program.EndTime;
    recRule.SeriesId = program.SeriesId;
    recRule.ProgramId = program.ProgramId;
    recRule.ChanId = channel.ChanId;
    recRule.CallSign = channel.CallSign;
    recRule.Season = program.Season;
    recRule.Episode = program.Episode;
    let date = new Date(recRule.StartTime);
    recRule.FindDay = (date.getDay() + 1) % 7;
    recRule.FindTime = date.toTimeString().slice(0, 8);
  }


  formatDate(date?: string): string {
    if (!date)
      return '';
    if (date.length == 10)
      date = date + ' 00:00';
    return new Date(date).toLocaleDateString()
  }

  formatTime(date?: string): string {
    if (!date)
      return '';
    // Get the locale specific time and remove the seconds
    const t = new Date(date);
    const tWithSecs = t.toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  filterFromRec(recRule: RecRule) {
    this.selectedFilters = [];
    this.recRuleFilters.forEach((filter) => {
      if ((recRule.Filter & (1 << filter.Id)) != 0)
        this.selectedFilters.push(filter.Id)
    });
  }

  filterToRec(recRule: RecRule) {
    recRule.Filter = 0;
    this.selectedFilters.forEach((id) => {
      recRule.Filter |= (1 << id);
    });
  }

  postProcFromRec(recRule: RecRule) {
    this.selectedPostProc = [];
    let arr = <BoolAssociativeArray><unknown>recRule;
    this.postProcList.forEach((entry) => {
      if (arr[entry.value])
        this.selectedPostProc.push(entry.value);
    });
  }

  postProcToRec(recRule: RecRule) {
    let arr = <BoolAssociativeArray><unknown>recRule;
    // clear existing entries
    this.postProcList.forEach((entry) => arr[entry.value] = false);
    // set new values
    this.selectedPostProc.forEach((entry) => arr[entry] = true);
  }

  iCheckbox(recRule: RecRule) {
    if (!recRule.InetRef)
      recRule.InetRef = '';
    if (recRule.InetRef.includes('.py_'))
      recRule.InetRef = recRule.InetRef.replace(/^.*\.py_/, this.metaPrefix);
    else
      recRule.InetRef = this.metaPrefix + recRule.InetRef;
  }

  iText(recRule: RecRule) {
    this.metaPrefix = recRule.InetRef.replace(/_.*/, '_');
    if (!this.metaPrefix.endsWith('.py_'))
      this.metaPrefix = '';
  }

  templateChange(recRule: RecRule) {
    const newTemplate = this.templates.find(entry => entry.Id == this.templateId);
    if (newTemplate)
      this.mergeTemplate(recRule, newTemplate);
    this.templateId = 0;
  }

  mergeTemplate(recRule: RecRule, template: RecRule) {
    recRule.Inactive = template.Inactive;
    recRule.RecPriority = template.RecPriority;
    recRule.PreferredInput = template.PreferredInput;
    recRule.StartOffset = template.StartOffset;
    recRule.EndOffset = template.EndOffset;
    recRule.DupMethod = template.DupMethod;
    recRule.DupIn = template.DupIn;
    recRule.NewEpisOnly = template.NewEpisOnly;
    recRule.Filter = template.Filter;
    recRule.RecProfile = template.RecProfile;
    recRule.RecGroup = template.RecGroup;
    recRule.StorageGroup = template.StorageGroup;
    recRule.PlayGroup = template.PlayGroup;
    recRule.AutoExpire = template.AutoExpire;
    recRule.MaxEpisodes = template.MaxEpisodes;
    recRule.MaxNewest = template.MaxNewest;
    recRule.AutoCommflag = template.AutoCommflag;
    recRule.AutoMetaLookup = template.AutoMetaLookup;
    recRule.AutoTranscode = template.AutoTranscode;
    recRule.AutoUserJob1 = template.AutoUserJob1;
    recRule.AutoUserJob2 = template.AutoUserJob2;
    recRule.AutoUserJob3 = template.AutoUserJob3;
    recRule.AutoUserJob4 = template.AutoUserJob4;
    recRule.Transcoder = template.Transcoder;
    this.postProcFromRec(recRule);
    this.filterFromRec(recRule);
  }


  close() {

    if (!this.recRule) {
      this.displayDlg = false;
      return;
    }
    if (this.recRule.Id == 0 && this.recRule.Type == 'Not Recording') {
      this.displayDlg = false;
      return;
    }
    if (this.currentForm.dirty) {
      if (this.displayUnsaved) {
        // Close on the unsaved warning
        this.displayUnsaved = false;
        this.displayDlg = false;
        this.currentForm.form.markAsPristine();
      }
      else
        // Close on the edit dialog
        // Open the unsaved warning
        this.displayUnsaved = true;
    }
    else {
      // Close on the edit dialog
      this.displayDlg = false;
      this.displayUnsaved = false;
    };
  }

  // good response to add: {"uint": 19}
  saveObserver = {
    next: (x: any) => {
      if (this.recRule) {
        if (this.recRule.Id && x.bool) {
          this.successCount++;
          this.currentForm.form.markAsPristine();
          setTimeout(() => this.inter.summaryComponent.refresh(), 3000);
          // after a deletion clear out the id so the next save
          // would add a new one.
          if (this.recRule.Type == 'Not Recording')
            this.recRule.Id = 0;
        }
        else if (!this.recRule.Id && x.uint) {
          this.successCount++;
          this.currentForm.form.markAsPristine();
          setTimeout(() => this.inter.summaryComponent.refresh(), 3000);
          this.recRule.Id = x.uint;
        }
        else {
          this.errorCount++;
          this.currentForm.form.markAsDirty();
        }
      }
      else {
        // Should not happen
        console.log("ERROR: recRule is undefined")
        this.errorCount++;
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      this.currentForm.form.markAsDirty();
    },
  };

  save() {
    if (!this.recRule)
      return;
    if (this.recRule.Id == 0 && this.recRule.Type == 'Not Recording')
      return;
    this.errorCount = 0;
    this.successCount = 0;
    if (this.recRule.Id > 0 && this.recRule.Type == 'Not Recording')
      this.dvrService.RemoveRecordSchedule(this.recRule.Id)
        .subscribe(this.saveObserver);
    else if (this.recRule.Id > 0) {
      const request = this.createRequest(this.recRule);
      request.RecordId = this.recRule.Id;
      this.dvrService.UpdateRecordSchedule(request)
        .subscribe(this.saveObserver);
    }
    else {
      const request = this.createRequest(this.recRule);
      this.dvrService.AddRecordSchedule(request)
        .subscribe(this.saveObserver);
    }
  }

  createRequest(recRule: RecRule): RecordScheduleRequest {
    const ret: RecordScheduleRequest = {
      Title: recRule.Title,
      Subtitle: recRule.SubTitle,
      Description: recRule.Description,
      Category: recRule.Category,
      StartTime: recRule.StartTime,
      EndTime: recRule.EndTime,
      SeriesId: recRule.SeriesId,
      ProgramId: recRule.ProgramId,
      ChanId: recRule.ChanId,
      Station: recRule.CallSign,
      FindDay: recRule.FindDay,
      FindTime: recRule.FindTime,
      ParentId: recRule.ParentId,
      Inactive: recRule.Inactive,
      Season: recRule.Season,
      Episode: recRule.Episode,
      InetRef: recRule.InetRef,
      Type: recRule.Type,
      SearchType: recRule.SearchType,
      RecPriority: recRule.RecPriority,
      PreferredInput: recRule.PreferredInput,
      StartOffset: recRule.StartOffset,
      EndOffset: recRule.EndOffset,
      LastRecorded: recRule.LastRecorded,
      DupMethod: recRule.DupMethod,
      DupIn: recRule.DupIn,
      NewEpisOnly: recRule.NewEpisOnly,
      Filter: recRule.Filter,
      RecProfile: recRule.RecProfile,
      RecGroup: recRule.RecGroup,
      StorageGroup: recRule.StorageGroup,
      PlayGroup: recRule.PlayGroup,
      AutoExpire: recRule.AutoExpire,
      MaxEpisodes: recRule.MaxEpisodes,
      MaxNewest: recRule.MaxNewest,
      AutoCommflag: recRule.AutoCommflag,
      AutoTranscode: recRule.AutoTranscode,
      AutoMetaLookup: recRule.AutoMetaLookup,
      AutoUserJob1: recRule.AutoUserJob1,
      AutoUserJob2: recRule.AutoUserJob2,
      AutoUserJob3: recRule.AutoUserJob3,
      AutoUserJob4: recRule.AutoUserJob4,
      Transcoder: recRule.Transcoder,
    };
    return ret;
  }

  URLencode(x: string): string {
    return encodeURI(x);
  }

  confirm(message?: string): Observable<boolean> {
    const confirmation = window.confirm(message);
    return of(confirmation);
  };

  canDeactivate(): Observable<boolean> | boolean {
    if (this.currentForm && this.currentForm.dirty)
      return this.confirm(this.translate.instant('settings.common.warning'));
    return true;
  }

  @HostListener('window:beforeunload', ['$event'])
  onWindowClose(event: any): void {
    if (this.currentForm && this.currentForm.dirty) {
      event.preventDefault();
      event.returnValue = false;
    }
  }



}
