import { Component, OnInit } from '@angular/core';
import { DvrService } from 'src/app/services/dvr.service';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';
import { SetupWizardService } from 'src/app/services/setupwizard.service';

@Component({
  selector: 'app-backend-warning',
  templateUrl: './backend-warning.component.html',
  styleUrls: ['./backend-warning.component.css']
})
export class BackendWarningComponent implements OnInit {

  errorCount = 0;
  retryCount = 0;
  upComing: ScheduleOrProgram[] = [];
  recStatusDesc = '';
  recStartTime = '';
  ready = false;
  delay = 0;
  busy = false;
  hostName = '';
  masterServerName = '';

  constructor(private mythService: MythService, public setupService: SetupService,
    private dvrService: DvrService, private wizardService: SetupWizardService) {
    this.getBackendInfo();
    this.refreshInfo();
  }

  refreshInfo() {
    setTimeout(() => {
      this.getBackendInfo();
      this.refreshInfo();
    }, 120000);
  }

  getBackendInfo() {
    this.errorCount = 0;
    this.ready = false;
    this.recStatusDesc = '';
    this.recStartTime = '';
    this.upComing = [];
    this.mythService.GetHostName().subscribe({
      next: data => {
        this.hostName = data.String;
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MasterServerName", Default: this.hostName })
          .subscribe({
            next: data => {
              this.masterServerName = data.String;
            },
            error: () => this.errorCount++
          });
      },
      error: () => this.errorCount++
    })
    this.mythService.GetBackendInfo()
      .subscribe({
        next: data => {
          this.setupService.schedulingEnabled = data.BackendInfo.Env.SchedulingEnabled;
          this.setupService.isDatabaseIgnored = data.BackendInfo.Env.IsDatabaseIgnored;
          this.setupService.DBTimezoneSupport = data.BackendInfo.Env.DBTimezoneSupport;
          if (this.setupService.isDatabaseIgnored)
            this.wizardService.wizardItems = this.wizardService.dbSetupMenu;
          else
            this.wizardService.wizardItems = this.wizardService.fullMenu;
          this.wizardService.getWizardData();
          this.retryCount = 0;
          setTimeout(() => this.getUpcoming(), this.delay);
          this.delay = 0;
        },
        error: () => {
          // default to true in case backend is down
          this.setupService.schedulingEnabled = true;
          this.errorCount++;
          if (this.errorCount < this.retryCount)
            setTimeout(() => this.getBackendInfo(), 5000);
          else
            this.retryCount = 0;
        }
      });
  }

  getUpcoming() {
    this.errorCount = 0;
    this.dvrService.GetUpcomingList({ Count: 1 })
      .subscribe({
        next: data => {
          this.upComing = data.ProgramList.Programs;
          this.ready = true;
          if (this.upComing.length > 0) {
            this.dvrService.RecStatusToString(this.upComing[0].Recording.Status)
              .subscribe(({
                next: data => this.recStatusDesc = data.String,
                error: () => this.errorCount++
              }));
            var d = new Date(this.upComing[0].Recording.StartTs);
            this.recStartTime = d.toLocaleString();
          }
        },
        error: () => this.errorCount++
      })
  }

  disableSched() {
    this.mythService.ManageScheduler({ Disable: true })
      .subscribe({
        next: data => this.getBackendInfo(),
        error: () => this.errorCount++
      });
  }

  restart() {
    this.mythService.Shutdown({ Restart: true })
      .subscribe({
        next: data => {
          if (data.bool) {
            this.retryCount = 5;
            this.getBackendInfo();
          }
          else
            this.errorCount++;
        },
        error: () => this.errorCount++
      });
    this.delay = 5000;
  }

  ngOnInit(): void {
  }

}
