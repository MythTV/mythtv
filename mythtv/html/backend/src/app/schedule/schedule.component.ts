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
import { UtilityService } from '../services/utility.service';
import { ChannelService } from '../services/channel.service';
import { OverlayPanel } from 'primeng/overlaypanel';
import { GuideService } from '../services/guide.service';

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
  inactive?: boolean;
}

interface BoolAssociativeArray {
  [key: string]: boolean
}

interface MyChannel extends Channel {
  Description?: string;
}

@Component({
  selector: 'app-schedule',
  templateUrl: './schedule.component.html',
  styleUrls: ['./schedule.component.css']
})
export class ScheduleComponent implements OnInit {
  @Input() inter!: ScheduleLink;
  @ViewChild("schedform") currentForm!: NgForm;
  @ViewChild("overlay") overlay!: OverlayPanel;

  displayDlg = false;
  displayUnsaved = false;
  loadCount = 0;
  successCount = 0;
  errorCount = 0;
  errortext = '';
  srchTypeDisabled = true;
  titleRows = 1;
  subTitleRows = 1;
  descriptionRows = 5;
  override = false;
  program?: ScheduleOrProgram;
  channel?: Channel;
  recRule?: RecRule;
  defaultTemplate?: RecRule;
  reqProgram?: ScheduleOrProgram
  reqChannel?: Channel
  reqRecRule?: RecRule
  reqDate = new Date();
  reqDuration = 60;
  metaPrefix = '';
  templateId = 0;
  neverRecord = false;
  schedTypeDisabled = false;
  newRuleType: string | undefined;

  htmlRegex = new RegExp("<TITLE>|</TITLE>");

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
  allChannels: MyChannel[] = [];
  fullDetails: string = '';

  srchTypeList: ListEntry[] = [
    { prompt: this.translate.instant('recrule.srch_None'), value: 'None' },
    { prompt: this.translate.instant('recrule.srch_PowerSearch'), value: 'Power Search' },
    { prompt: this.translate.instant('recrule.srch_TitleSearch'), value: 'Title Search' },
    { prompt: this.translate.instant('recrule.srch_KeywordSearch'), value: 'Keyword Search' },
    { prompt: this.translate.instant('recrule.srch_PeopleSearch'), value: 'People Search' },
    { prompt: this.translate.instant('recrule.srch_ManualSearch'), value: 'Manual Search' },
  ]

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

  autoExtendList: ListEntry[] = [
    { prompt: this.translate.instant('dashboard.sched.extend_none'), value: 'None' },
    { prompt: 'ESPN', value: 'ESPN' },
    { prompt: 'MLB', value: 'MLB' },
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


  constructor(private dvrService: DvrService, private translate: TranslateService,
    private mythService: MythService, public utility: UtilityService, private channelService: ChannelService,
    private guideService: GuideService) {
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
    this.channelService.GetChannelInfoList({ Details: true }).subscribe(data => {
      this.allChannels = data.ChannelInfoList.ChannelInfos;
      this.allChannels.forEach(channel => channel.Description
        = channel.ChanNum + ' ' + channel.ChannelName + ' (' + channel.CallSign + ')')
      this.loadSuccess();
    });
  }

  loadSuccess() {
    this.loadCount++;
    if (this.loadCount == 11) {
      this.loadCount = 0;
      this.setupData();
      this.currentForm.form.markAsPristine();
      if (this.recRule && this.newRuleType) {
        if (this.newRuleType == 'Never Record' && this.recRule.SearchType == 'Manual Search') {
          this.errortext = this.translate.instant('dashboard.sched.invalid_never');
          this.errorCount = 1;
          this.displayDlg = true;
          return;
        }
        this.recRule.Type = this.newRuleType;
        this.recRule.Inactive = false;
        this.save();
      }
      else
        this.displayDlg = true;
    }
  }

  loadFail() {
    this.errorCount++;
  }

  open(program?: ScheduleOrProgram, channel?: Channel, recRule?: RecRule, newRuleType?: string) {
    this.program = undefined;
    this.channel = undefined;
    this.recRule = undefined;
    this.templateId = 0;
    this.reqProgram = program;
    this.reqChannel = channel;
    this.reqRecRule = recRule;
    this.newRuleType = newRuleType;
    this.titleRows = 1;
    this.neverRecord = false;
    this.successCount = 0;
    this.errorCount = 0;
    this.errortext = '';
    this.metaPrefix = '';
    if (program || recRule) {
      this.srchTypeDisabled = true;
      this.srchTypeList[0].inactive = false;
    }
    else {
      this.srchTypeDisabled = false;
      this.srchTypeList[0].inactive = true;
    }
    this.schedTypeDisabled = false;
    this.loadLists();
  }

  setupData() {
    let newOverride = false;
    this.program = this.reqProgram;
    let ruleType = '';
    if (this.reqRecRule)
      ruleType = this.reqRecRule.Type;
    if (this.reqRecRule) {
      if (['Override Recording', 'Do not Record'].indexOf(ruleType) > -1 && !this.reqRecRule.Id) {
        newOverride = true;
        this.recRule = undefined;
      }
      else
        // This works because RecRule only has elementary items.
        this.recRule = Object.assign({}, this.reqRecRule);
    }
    else
      this.recRule = undefined;
    this.channel = undefined;
    if (this.reqChannel)
      this.channel = this.reqChannel;
    else if (this.reqRecRule)
      this.channel = this.allChannels.find((entry) => entry.ChanId == this.reqRecRule?.ChanId);
    if (this.program) {
      if (!this.channel && this.program.Channel)
        this.channel = this.program.Channel;
    }

    var recId = 0;
    this.typeList = [];
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
        if (entry.Category == 'Default')
          this.defaultTemplate = entry;
      }
    });
    if (newOverride) {
      // This works because RecRule only has elementary items.
      this.recRule = Object.assign({}, this.recRule);
      this.recRule.ParentId = this.recRule.Id;
      this.recRule.Id = 0;
      if (this.recRule.SearchType != 'Manual Search')
        this.recRule.SearchType = 'None';
      ruleType = "Override Recording";
    }
    if (!this.recRule) {
      this.recRule = <RecRule>{ Id: 0 };
    }
    if (!this.recRule.Title) {
      this.recRule.Title = ''
      this.recRule.SubTitle = ''
      this.recRule.Description = ''
      if (this.defaultTemplate)
        this.mergeTemplate(this.recRule, this.defaultTemplate)
    }
    if (!this.recRule.SearchType)
      this.recRule.SearchType = 'None';

    if (this.program && this.channel)
      this.mergeProgram(this.recRule, this.program, this.channel);

    if (this.reqProgram?.Recording?.StatusName == 'NeverRecord') {
      this.neverRecord = true;
      ruleType = 'Never Record';
    }

    if (!ruleType)
      ruleType = 'Not Recording';

    if (!this.recRule.StartTime) {
      let date = new Date();
      this.recRule.StartTime = date.toISOString();
      this.recRule.FindDay = (date.getDay() + 1) % 7;
      this.recRule.FindTime = date.toTimeString().slice(0, 8);
    }

    this.filterFromRec(this.recRule);
    this.postProcFromRec(this.recRule);

    this.recRule.Type = ruleType;
    this.setupTypeList(this.recRule);

    if (!this.srchTypeDisabled)
      this.onSearchTypeChange();

    if (this.override)
      this.recRule.Inactive = false;

    if (!this.newRuleType) {
      setTimeout(() => {
        if (newOverride)
          ruleType = 'Not Recording';
        if (this.recRule)
          this.recRule.Type = ruleType;
        this.currentForm.form.markAsPristine();
      }, 10);
    }
  }

  setupTypeList(recRule: RecRule) {
    let ruleType = recRule.Type;
    this.typeList.length = 0;
    this.override = false;

    if (ruleType == 'Recording Template') {
      this.typeList.push(
        {
          prompt: this.translate.instant('dashboard.sched.type.mod_template'),
          value: 'Recording Template'
        }
      );
      if (recRule.Category != 'Default' && recRule.Id > 0) {
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.del_template'),
            value: 'Not Recording'
          }
        );
      }
    }
    else if (['Override Recording', 'Do not Record'].indexOf(ruleType) > -1
      || this.neverRecord) {
      this.override = true;
      if (this.neverRecord)
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.forget_history'),
            value: 'Forget History'
          }
        );
      else
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
          });
      if (this.recRule?.SearchType != 'Manual Search')
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.never_rec_override'),
            value: 'Never Record'
          }
        );

        // current recording = 3, prev recording = 2, never rec = 11
      if ( this.reqProgram?.Recording?.Status && [2, 3, 11].indexOf(this.reqProgram?.Recording?.Status) != -1)
        this.typeList.push(
          {
            prompt: this.translate.instant('dashboard.sched.type.forget_history'),
            value: 'Forget History'
          }
        );
    }
    else {
      const isManual = (recRule.SearchType == 'Manual Search');
      const isSearch = (recRule.SearchType != 'None');
      this.typeList.push(
        {
          prompt: this.translate.instant('dashboard.sched.type.not'),
          value: 'Not Recording'
        }
      );
      if (recRule.CallSign && !isSearch || isManual)
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
      if (!recRule.CallSign || isSearch)
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
    // If the selected entry is no longer in the available list, set it to the
    // first item in the list
    if (this.typeList.findIndex((x) => x.value == recRule.Type) == -1) {
      recRule.Type = this.typeList[0].value;
    }
  }


  mergeProgram(recRule: RecRule, program: ScheduleOrProgram, channel: Channel) {
    // In searches, these fields contain search and additional tables
    // so do not fill in the subtitle and description
    if (recRule.SearchType == 'None') {
      recRule.Title = program.Title;
      recRule.SubTitle = program.SubTitle;
      recRule.Description = program.Description;
    }
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
    if (!recRule.Inetref)
      recRule.Inetref = '';
    if (recRule.Inetref.includes('.py_'))
      recRule.Inetref = recRule.Inetref.replace(/^.*\.py_/, this.metaPrefix);
    else
      recRule.Inetref = this.metaPrefix + recRule.Inetref;
  }

  iText(recRule: RecRule) {
    this.metaPrefix = recRule.Inetref.replace(/_.*/, '_');
    if (!this.metaPrefix.endsWith('.py_'))
      this.metaPrefix = '';
  }

  templateChange(recRule: RecRule) {
    if (this.templateId) {
      const newTemplate = this.templates.find(entry => entry.Id == this.templateId);
      if (newTemplate)
        this.mergeTemplate(recRule, newTemplate);
    }
  }

  onSearchTypeChange() {
    if (this.recRule) {
      this.recRule.Title = '';
      this.reqDate = new Date();
      this.reqDate.setMinutes(0);
      this.reqDate.setSeconds(0, 0);
      this.reqDuration = 60;
      this.onDateChange();
      if (this.recRule.SearchType == "Manual Search") {
        this.recRule.Description = '';
        setTimeout(() => this.onChannelChange(), 100);
      }
      if (this.recRule.SearchType == "Power Search") {
        this.subTitleRows = 5;
      }
      else {
        this.subTitleRows = 1;
      }
      if (['None', 'Power Search'].indexOf(this.recRule.SearchType) > -1)
        this.descriptionRows = 5;
      else
        this.descriptionRows = 1;
      this.recRule.SubTitle = '';
      this.recRule.Description = '';
      this.setupTypeList(this.recRule);
    }
    this.onTitleBlur();
    this.onDescriptionBlur();
  }

  onTitleBlur() {
    if (this.recRule) {
      this.recRule.Title = this.recRule.Title.trim();
      if (this.recRule.Title.length > 0) {
        let desc;
        if (['Manual Search', 'Power Search'].indexOf(this.recRule.SearchType) > -1 && this.recRule.Title.length > 0)
          desc = '(' + this.trSearch(this.recRule.SearchType) + ')'
        if (this.recRule.Type == 'Recording Template' && this.recRule.Title.length > 0)
          desc = '(' + this.translate.instant('recrule.template') + ')'
        if (desc && this.recRule.Title.indexOf(desc) == -1)
          this.recRule.Title = this.recRule.Title + ' ' + desc;
      }
      if (this.recRule.SearchType == 'Manual Search')
        this.recRule.Description = this.recRule.Title;
    }
  }

  onDescriptionBlur() {
    if (this.recRule) {
      this.recRule.Description = this.recRule.Description.trim();
      if (['Title Search', 'Keyword Search', 'People Search'].indexOf(this.recRule.SearchType) > -1)
        if (this.recRule.Description.length > 0)
          this.recRule.Title = this.recRule.Description + ' (' + this.trSearch(this.recRule.SearchType) + ')';
        else
          this.recRule.Title = '';
    }
  }

  onChannelChange() {
    if (this.recRule && this.channel) {
      this.recRule.ChanId = this.channel.ChanId;
      this.recRule.CallSign = this.channel.CallSign;
    }
  }

  onDateChange() {
    if (this.recRule) {
      this.recRule.StartTime = this.reqDate.toISOString();
      this.recRule.FindDay = (this.reqDate.getDay() + 1) % 7;
      this.recRule.FindTime = this.reqDate.toTimeString().slice(0, 8);
      this.onDurationChange();
    }
  }

  onDurationChange() {
    if (this.recRule) {
      let start = new Date(this.recRule.StartTime);
      let end = new Date(start.getTime() + this.reqDuration * 60000);
      this.recRule.EndTime = end.toISOString();
    }
  }

  trSearch(value: string): string {
    return this.translate.instant('recrule.srch_' + value.replace(' ', ''));
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
    if (this.currentForm.dirty && !this.schedTypeDisabled) {
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
        if ((this.recRule.Id || this.recRule.Type == 'Never Record'
          || this.recRule.Type == 'Forget History') && x.bool) {
          this.successCount++;
          this.currentForm.form.markAsPristine();
          // After setting never record, disable further changes
          if (this.recRule.Type == 'Never Record' || this.neverRecord)
            this.schedTypeDisabled = true;
          setTimeout(() => this.inter.summaryComponent.refresh(), 1000);
          // after a deletion clear out the id so the next save
          // would add a new one.
          if (this.recRule.Type == 'Not Recording')
            this.recRule.Id = 0;
        }
        else if (!this.recRule.Id && x.uint) {
          this.successCount++;
          this.currentForm.form.markAsPristine();
          setTimeout(() => this.inter.summaryComponent.refresh(), 1000);
          this.recRule.Id = x.uint;
        }
        else {
          console.log("Unexpected response:", x, "this.recRule.Type:", this.recRule.Type);
          this.errorCount++;
          this.displayDlg = true;
          this.currentForm.form.markAsDirty();
        }
      }
      else {
        // Should not happen
        console.log("ERROR: recRule is undefined")
        this.errorCount++;
        this.displayDlg = true;
      }
    },
    error: (err: any) => {
      console.error(err);
      if (err.status == 400) {
        let parts = (<string>err.error).split(this.htmlRegex);
        if (parts.length > 1)
          this.errortext = parts[1];
      }
      this.errorCount++;
      this.displayDlg = true;
      this.currentForm.form.markAsDirty();
    },
  };

  detailsreq(event: any) {
    this.fullDetails = '';
    if (!this.recRule)
      return;
    this.guideService.GetProgramDetails({
      ChanId: this.recRule.ChanId,
      StartTime: this.recRule.StartTime
    }).subscribe((prog) => {
      let combo: any = {};
      let parms = [];
      if (this.program)
        combo = this.program;
      combo = { ...combo, ...this.recRule };
      for (const [key, value] of Object.entries(combo)) {
        if (value != null && typeof value != 'object')
          parms.push(`${key}: ${value}`);
      }
      parms.sort();
      if (prog.Program.Cast.CastMembers
        && prog.Program.Cast.CastMembers.length > 0) {
        parms.push('');
        parms.push(this.translate.instant('dashboard.sched.cast'));
        prog.Program.Cast.CastMembers.forEach((member) => {
          let chName = member.CharacterName ? member.CharacterName : member.TranslatedRole;
          parms.push(chName + ': ' + member.Name)
        });
      }
      this.fullDetails = parms.join('<br>');
      this.overlay.show(event);
    });
  }


  save() {
    this.errortext = '';
    if (!this.recRule)
      return;
    if (this.recRule.Type == 'Forget History') {
      if (this.program && this.channel) {
        this.dvrService.AllowReRecord(undefined, this.channel.ChanId,
          this.program.StartTime).subscribe(this.saveObserver);
      }
      return;
    }
    if (this.neverRecord) {
      // For a neverRecord situation the only valid entry is "Forget History"
      // so if it gets here there is nothing to do.
      this.currentForm.form.markAsPristine();
      return;
    }
    if (this.recRule.Type == 'Never Record') {
      this.dvrService.AddDontRecordSchedule({
        ChanId: this.recRule.ChanId,
        StartTime: this.recRule.StartTime,
        NeverRecord: true
      }).subscribe(this.saveObserver);
      return;
    }
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
      Inetref: recRule.Inetref,
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
      AutoExtend: recRule.AutoExtend
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
