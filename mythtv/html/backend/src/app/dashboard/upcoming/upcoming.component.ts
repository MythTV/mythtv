import { Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MenuItem, MessageService } from 'primeng/api';
import { Menu } from 'primeng/menu';
import { PartialObserver } from 'rxjs';
import { ScheduleLink } from 'src/app/schedule/schedule.component';
import { DataService } from 'src/app/services/data.service';
import { DvrService } from 'src/app/services/dvr.service';
import { ProgramList, ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';

@Component({
  selector: 'app-upcoming',
  templateUrl: './upcoming.component.html',
  styleUrls: ['./upcoming.component.css'],
  providers: [MessageService]
})
export class UpcomingComponent implements OnInit {

  @ViewChild("upcomingform") currentForm!: NgForm;
  @ViewChild("menu") menu!: Menu;

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

  formatDate(date: string): string {
    if (!date)
      return '';
    if (date.length == 10)
      date = date + ' 00:00';
    return new Date(date).toLocaleDateString()
  }

  formatTime(date: string): string {
    if (!date)
      return '';
    // Get the locale specific time and remove the seconds
    const t = new Date(date);
    const tWithSecs = t.toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

  getDuration(program: ScheduleOrProgram): number {
    const starttm = new Date(program.Recording.StartTs).getTime();
    const endtm = new Date(program.Recording.EndTs).getTime();
    const duration = (endtm - starttm) / 60000;
    return duration;
  }

  updateRecRule(program: ScheduleOrProgram) {
    let copyProgram = Object.assign({}, program);
    if (this.inter.sched)
      this.inter.sched.open(copyProgram);
  }

}
