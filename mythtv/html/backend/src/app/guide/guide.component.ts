import { Component, OnInit } from '@angular/core';
import { Observable } from 'rxjs';
import { GuideService } from 'src/app/services/guide.service';
import { Channel } from '../services/interfaces/channel.interface';
import { ProgramGuide } from 'src/app/services/interfaces/programguide.interface';
import { TranslateService, TranslationChangeEvent, TranslatePipe } from '@ngx-translate/core';
import { ScheduleLink, SchedulerSummary, ScheduleComponent } from '../schedule/schedule.component';
import { ScheduleOrProgram } from '../services/interfaces/program.interface';
import { GetProgramListRequest } from '../services/interfaces/guide.interface';
import { ChannelGroup } from '../services/interfaces/channelgroup.interface';
import { ActivatedRoute } from '@angular/router';
import { NgIf, NgFor } from '@angular/common';
import { DropdownModule } from 'primeng/dropdown';
import { FormsModule } from '@angular/forms';
import { Tooltip } from 'primeng/tooltip';
import { Calendar } from 'primeng/calendar';
import { ButtonDirective } from 'primeng/button';
import { Ripple } from 'primeng/ripple';
import { Checkbox } from 'primeng/checkbox';
import { ProgressSpinner } from 'primeng/progressspinner';
import { DataView } from 'primeng/dataview';
import { PrimeTemplate } from 'primeng/api';
import { TimebarComponent } from './components/timebar/timebar.component';
import { ChannelIconComponent } from './components/channelicon/channelicon.component';
import { ProgramEntryComponent } from './components/programentry/programentry.component';
import { ProgramsComponent } from '../dashboard/programs/programs.component';
import { Dialog } from 'primeng/dialog';
import { LegendComponent } from './components/legend/legend.component';

@Component({
    selector: 'app-guide',
    templateUrl: './guide.component.html',
    styleUrls: ['./guide.component.css'],
    standalone: true,
    imports: [NgIf, DropdownModule, FormsModule, Tooltip, Calendar, ButtonDirective, Ripple, Checkbox, ProgressSpinner, DataView, PrimeTemplate, NgFor, TimebarComponent, ChannelIconComponent, ProgramEntryComponent, ProgramsComponent, ScheduleComponent, Dialog, LegendComponent, TranslatePipe]
})
export class GuideComponent implements OnInit, SchedulerSummary {


  m_guideData$!: Observable<ProgramGuide>;
  m_startDate: Date = new Date();
  m_pickerDate: Date = new Date();
  m_endDate: Date = new Date();
  channelGroups: ChannelGroup[] = [];
  m_channelData: Channel[] = [];
  m_channelTotal: number = 10;
  m_rows: number = 10;
  m_programGuide!: ProgramGuide;
  listPrograms: ScheduleOrProgram[] = [];
  allGroup: ChannelGroup = {
    GroupId: 0,
    Name: this.translate.instant('settings.chanedit.all'),
    Password: ''
  }
  channelGroup: ChannelGroup = this.allGroup;
  channel!: Channel;
  loaded = false;
  refreshing = false;
  timeChange = false;
  inter: ScheduleLink = { summaryComponent: this };
  // display Type
  readonly GRID = 1;
  readonly CHANNEL = 2;
  readonly TITLESEARCH = 3;
  readonly PEOPLESEARCH = 4;
  readonly FULLSEARCH = 5;
  readonly CATSEARCH = 6;
  readonly ANYSEARCH = 7;
  displayType = this.GRID;
  searchValue = '';
  showLegend = false;
  startChanid?: number;
  startTime?: Date;
  startSchedule?: boolean;
  startGroup?: number;
  onlyNew = false;
  onlyMovies = false;
  groupByList = [
    { Value: 'None', Name: 'dashboard.guide.groupby.None' },
    { Value: 'ChanNum', Name: 'dashboard.guide.groupby.ChanNum' },
    { Value: 'CallSign', Name: 'dashboard.guide.groupby.CallSign' },
    { Value: 'ProgramId', Name: 'dashboard.guide.groupby.ProgramId' },
  ];
  groupBy = this.groupByList[0];


  constructor(private guideService: GuideService, private route: ActivatedRoute,
    private translate: TranslateService) {
    this.translate.onLangChange.subscribe((event: TranslationChangeEvent) => {
      console.log("Event: language change, new language (" + event.lang + ")");
      this.switchLanguage(event.lang);
    });
    this.groupByList.forEach(element => {
      this.translate.get(element.Name).subscribe(data => {
        element.Name = data;
      });
    });
    let wantedId = localStorage.getItem("GuideGroupBy");
    if (wantedId) {
      let groupBy = this.groupByList.find( element => element.Value == wantedId );
      if (groupBy)
        this.groupBy = groupBy;
    }
    localStorage.setItem("GuideGroupBy", this.groupBy.Value);
  }


  ngOnInit(): void {
    this.startChanid = this.route.snapshot.queryParams.Chanid;
    let reqDate;
    reqDate = new Date(this.route.snapshot.queryParams.StartTime);
    if (Number.isNaN(reqDate.valueOf())) {
      reqDate = new Date(Number(this.route.snapshot.queryParams.StartTime));
    }
    if (Number.isNaN(reqDate.valueOf()))
      reqDate = undefined;
    if (reqDate)
      this.startTime = reqDate;
    else
      this.startTime = undefined;
    this.startSchedule = this.route.snapshot.queryParams.Schedule;
    this.fetchData(this.startTime);
  }

  switchLanguage(language: string): void {
    this.translate.use(language);
  }

  fetchData(reqDate?: Date): void {
    if (this.channelGroups.length == 0) {
      this.guideService.GetChannelGroupList(false).subscribe(
        data => {
          this.channelGroups = data.ChannelGroupList.ChannelGroups;
          this.channelGroups.unshift(this.allGroup);
          let wantedGroup = localStorage.getItem("ChannelGroup");
          if (!wantedGroup)
            wantedGroup = this.allGroup.Name;
          if (this.route.snapshot.queryParams.ChannelGroup)
            wantedGroup = this.route.snapshot.queryParams.ChannelGroup;
          let group = this.channelGroups.find((entry) =>
            entry.Name == wantedGroup);
          if (group)
            this.channelGroup = group;
          localStorage.setItem("ChannelGroup", this.channelGroup.Name);
          this.fetchGuide(reqDate);
        });
    }
    else
      this.fetchGuide(reqDate);
  }

  fetchGuide(reqDate?: Date) {
    this.guideService.GetProgramGuide(reqDate, this.channelGroup.GroupId).subscribe(data => {
      this.m_programGuide = data;
      this.m_startDate = new Date(data.ProgramGuide.StartTime);
      this.m_pickerDate = new Date(this.m_startDate);
      this.m_endDate = new Date(data.ProgramGuide.EndTime);
      this.m_channelData = data.ProgramGuide.Channels;
      this.m_channelTotal = data.ProgramGuide.TotalAvailable;
      this.loaded = true;
      this.refreshing = false;
      this.timeChange = false;
      if (this.startChanid) {
        setTimeout(() => {
          const element = document.getElementById('Chan' + this.startChanid);
          element?.scrollIntoView();
          if (this.startSchedule) {
            let chan = this.m_programGuide.ProgramGuide.Channels
              .find((entry) => entry.ChanId == this.startChanid);
            if (chan) {
              let prog = chan.Programs.find((entry) =>
                this.startTime?.valueOf() == new Date(entry.StartTime).valueOf());
              if (prog)
                this.inter.sched?.open(prog, chan);
            }
          }
          this.startChanid = undefined;
        }, 100);
      }
    });
  }

  fetchDetails() {
    // Ask for a time 1 second after selected time
    // Otherwise gives you shows that end at at that time also
    let millisecs = this.m_startDate.getTime();
    let startDate = new Date(millisecs + 1000);
    let request: GetProgramListRequest = {
      Details: true,
      StartTime: startDate.toISOString(),
      GroupBy: this.groupBy.Value
    };
    if (this.searchValue == "") {
      if (this.onlyMovies || this.onlyNew)
        this.displayType = this.ANYSEARCH
    }
    switch (this.displayType) {
      case this.CHANNEL:
        request.ChanId = this.channel.ChanId;
        break;
      case this.TITLESEARCH:
        request.TitleFilter = this.searchValue;
        break;
      case this.PEOPLESEARCH:
        request.PersonFilter = this.searchValue;
        break;
      case this.FULLSEARCH:
        request.KeywordFilter = this.searchValue;
        break;
      case this.CATSEARCH:
        request.CategoryFilter = this.searchValue;
        break;
      case this.ANYSEARCH:
        break;
    }
    request.OnlyNew = this.onlyNew;
    if (this.onlyMovies)
      request.CatType = 'movie';
    request.Count = 10000;
    this.listPrograms = [];
    this.guideService.GetProgramList(request).subscribe(data => {
      this.listPrograms = data.ProgramList.Programs;
      this.loaded = true;
      this.refreshing = false;
    });
  }

  inDisplayWindow(startTime: string, endTime: string): boolean {
    let p_start = new Date(startTime);
    let p_end = new Date(endTime);
    let w_start = new Date(this.m_startDate);
    let w_end = new Date(this.m_endDate);
    if (p_end <= w_start) {
      return false;
    }
    if (p_start >= w_end) {
      return false;
    }
    return (p_start < w_end);
  }

  onDateChange(): void {
    if (!this.m_pickerDate)
      this.m_pickerDate = new Date();
    if (this.m_pickerDate.getTime() == this.m_startDate.getTime())
      return;
    this.m_startDate = new Date(this.m_pickerDate);
    this.timeChange = true;
    this.refresh();
  }

  refresh(): void {
    this.refreshing = true;
    localStorage.setItem("ChannelGroup", this.channelGroup.Name);
    localStorage.setItem("GuideGroupBy", this.groupBy.Value);
    switch (this.displayType) {
      case this.GRID:
        if (this.m_startDate) {
          this.refreshing = true;
          this.fetchData(this.m_startDate);
        }
        break;
      case this.CHANNEL:
      case this.TITLESEARCH:
      case this.PEOPLESEARCH:
      case this.FULLSEARCH:
      case this.CATSEARCH:
      case this.ANYSEARCH:
        this.refreshing = true;
        this.fetchDetails();
        break;
    }
  }

  keydown(event: KeyboardEvent ) {
    if (event.key == "Enter")
      this.titleSearch();
  }

  onChannel(channel: Channel) {
    this.channel = channel;
    this.displayType = this.CHANNEL;
    this.refresh();
  }

  onGrid() {
    this.displayType = this.GRID;
    this.onlyNew = false;
    this.onlyMovies = false;
    this.refresh();
  }

  titleSearch() {
    this.searchValue = this.searchValue.trim();
    if (this.searchValue.length > 1) {
      this.displayType = this.TITLESEARCH;
      this.refresh();
    }
    else
      this.anySearch();
  }

  peopleSearch() {
    this.searchValue = this.searchValue.trim();
    if (this.searchValue.length > 1) {
      this.displayType = this.PEOPLESEARCH;
      this.refresh();
    }
    else
      this.anySearch();
  }

  fullSearch() {
    this.searchValue = this.searchValue.trim();
    if (this.searchValue.length > 1) {
      this.displayType = this.FULLSEARCH;
      this.refresh();
    }
    else
      this.anySearch();
  }

  anySearch() {
    if (this.onlyMovies || this.onlyNew || this.searchValue) {
      this.refresh();
    }
    else
      this.onGrid();
  }

  catSearch() {
    this.searchValue = this.searchValue.trim();
    if (this.searchValue.length > 1) {
      this.displayType = this.CATSEARCH;
      this.refresh();
    }
    else
      this.anySearch();
  }


  newSearch() {
    if (this.displayType == this.GRID) {
      this.displayType = this.ANYSEARCH;
      this.onlyNew = true;
      this.refresh();
    } else
      this.anySearch();
  }

  movieSearch() {
    if (this.displayType == this.GRID) {
      this.displayType = this.ANYSEARCH;
      this.onlyMovies = true;
      this.refresh();
    } else
      this.anySearch();
  }

}
