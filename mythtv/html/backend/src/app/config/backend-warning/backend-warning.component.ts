import { Component, OnInit } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';
import { MessageService } from 'primeng/api';
import { DvrService } from 'src/app/services/dvr.service';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';
import { SetupWizardService } from 'src/app/services/setupwizard.service';

@Component({
  selector: 'app-backend-warning',
  templateUrl: './backend-warning.component.html',
  styleUrls: ['./backend-warning.component.css'],
  providers: [MessageService]
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
    private dvrService: DvrService, private wizardService: SetupWizardService,
    private messageService: MessageService, private translate: TranslateService) {
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
    if (this.retryCount == 0)
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
          this.setupService.WebOnlyStartup = data.BackendInfo.Env.WebOnlyStartup;
          // Sometimes conflicting results are returned by the backend, saying that
          // scheduling is enabled when it could not be beacuse there is no database.
          if (this.setupService.isDatabaseIgnored || !this.setupService.DBTimezoneSupport
            || this.setupService.WebOnlyStartup != 'NONE')
            this.setupService.schedulingEnabled = false;
          if (this.setupService.isDatabaseIgnored)
            this.wizardService.wizardItems = this.wizardService.dbSetupMenu;
          else
            this.wizardService.wizardItems = this.wizardService.fullMenu;
          this.wizardService.getWizardData();
          if (this.retryCount > 0 && this.errorCount > 0) {
            // a succssful restart was done
            this.retryCount = 0;
            this.errorCount = 0;
          }
          else if (this.retryCount > 0 && this.errorCount == 0)
            // we are waiting for the shutdown that is
            // part of a restart
            setTimeout(() => this.getBackendInfo(), 2000);
          if (this.retryCount == 0) {
            // successful restart or no restart
            setTimeout(() => this.getUpcoming(), this.delay);
            this.delay = 0;
          }
        },
        error: () => {
          this.errorCount++;
          if (this.errorCount < this.retryCount)
            // shutdowsn doen, waiting for restart
            setTimeout(() => this.getBackendInfo(), 2000);
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

  restart(webOnly: boolean) {
    this.mythService.Shutdown({ Restart: true, WebOnly: webOnly })
      .subscribe({
        next: data => {
          if (data.bool) {
            // each retry generates 2 errors
            // this retry count approximates to number of seconds
            this.retryCount = 30;
            this.getBackendInfo();
          }
          else
            this.errorCount++;
          // }
        },
        error: () => this.errorCount++
      });
    this.delay = 5000;
  }

  ngOnInit(): void {
  }

}
