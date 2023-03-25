import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-shutdown-wakeup',
  templateUrl: './shutdown-wakeup.component.html',
  styleUrls: ['./shutdown-wakeup.component.css']
})
export class ShutdownWakeupComponent implements OnInit, AfterViewInit {

  @ViewChild("shutwakeopt")
  currentForm!: NgForm;

  successCount = 0;
  errorCount = 0;
  startupCommand = "";
  blockSDWUwithoutClient = true;
  idleTimeoutSecs = 0;
  idleWaitForRecordingTime = 15;
  StartupSecsBeforeRecording = 120;
  WakeupTimeFormat = "hh =mm yyyy-MM-dd";
  SetWakeuptimeCommand = "";
  ServerHaltCommand = "sudo /sbin/halt -p";
  preSDWUCheckCommand = "";

  constructor(public setupService: SetupService, private mythService: MythService) {
    this.getShutWake();
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  getShutWake() {

    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "startupCommand", Default: "" })
      .subscribe({
        next: data => this.startupCommand = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "blockSDWUwithoutClient", Default: "1" })
      .subscribe({
        next: data => this.blockSDWUwithoutClient = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "idleTimeoutSecs", Default: "0" })
      .subscribe({
        next: data => this.idleTimeoutSecs = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "idleWaitForRecordingTime", Default: "" })
      .subscribe({
        next: data => this.idleWaitForRecordingTime = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "StartupSecsBeforeRecording", Default: "120" })
      .subscribe({
        next: data => this.StartupSecsBeforeRecording = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WakeupTimeFormat", Default: "hh:mm yyyy-MM-dd" })
      .subscribe({
        next: data => this.WakeupTimeFormat = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "SetWakeuptimeCommand", Default: "" })
      .subscribe({
        next: data => this.SetWakeuptimeCommand = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "ServerHaltCommand", Default: "sudo /sbin/halt -p" })
      .subscribe({
        next: data => this.ServerHaltCommand = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "preSDWUCheckCommand", Default: "" })
      .subscribe({
        next: data => this.preSDWUCheckCommand = data.String,
        error: () => this.errorCount++
      });
  }

  swObserver = {
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
      HostName: '_GLOBAL_', Key: "startupCommand",
      Value: this.startupCommand
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "blockSDWUwithoutClient",
      Value: this.blockSDWUwithoutClient ? "1" : "0"
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "idleTimeoutSecs",
      Value: String(this.idleTimeoutSecs)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "idleWaitForRecordingTime",
      Value: String(this.idleWaitForRecordingTime)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "StartupSecsBeforeRecording",
      Value: String(this.StartupSecsBeforeRecording)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "WakeupTimeFormat",
      Value: this.WakeupTimeFormat
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "SetWakeuptimeCommand",
      Value: this.SetWakeuptimeCommand
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "ServerHaltCommand",
      Value: this.ServerHaltCommand
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "preSDWUCheckCommand",
      Value: this.preSDWUCheckCommand
    }).subscribe(this.swObserver);
  }

}
