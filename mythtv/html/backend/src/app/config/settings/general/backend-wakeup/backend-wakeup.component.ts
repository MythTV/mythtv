import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-backend-wakeup',
  templateUrl: './backend-wakeup.component.html',
  styleUrls: ['./backend-wakeup.component.css']
})
export class BackendWakeupComponent implements OnInit, AfterViewInit {

  @ViewChild("backendwakeup")
  currentForm!: NgForm;

  hostName = '';
  successCount = 0;
  errorCount = 0;
  WOLbackendReconnectWaitTime = 0;
  WOLbackendConnectRetry = 5;
  WOLbackendCommand = "";
  SleepCommand = "";
  WakeUpCommand = "";

  constructor(public setupService: SetupService, private mythService: MythService) {

  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  getBackendWake() {
    this.mythService.GetHostName().subscribe({
      next: data => {
        this.hostName = data.String;
        this.getSettings();
      },
      error: () => this.errorCount++
    })

  }

  getSettings() {
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WOLbackendReconnectWaitTime", Default: "0" })
      .subscribe({
        next: data => this.WOLbackendReconnectWaitTime = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WOLbackendConnectRetry", Default: "5" })
      .subscribe({
        next: data => this.WOLbackendConnectRetry = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WOLbackendCommand", Default: "" })
      .subscribe({
        next: data => this.WOLbackendCommand = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "SleepCommand", Default: "" })
      .subscribe({
        next: data => this.SleepCommand = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "WakeUpCommand", Default: "" })
      .subscribe({
        next: data => this.WakeUpCommand = data.String,
        error: () => this.errorCount++
      });

  }


  bewObserver = {
    next: (x: any) => {
      if (x.bool)
        this.successCount++;
      else {
        this.errorCount++;
        if (this.currentForm)
          this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      if (this.currentForm)
        this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.successCount = 0;
    this.errorCount = 0;
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "WOLbackendReconnectWaitTime",
      Value: String(this.WOLbackendReconnectWaitTime)
    }).subscribe(this.bewObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "WOLbackendConnectRetry",
      Value: String(this.WOLbackendConnectRetry)
    }).subscribe(this.bewObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "WOLbackendCommand",
      Value: this.WOLbackendCommand
    }).subscribe(this.bewObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "SleepCommand",
      Value: this.SleepCommand
    }).subscribe(this.bewObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "WakeUpCommand",
      Value: this.WakeUpCommand
    }).subscribe(this.bewObserver);
  }


}
