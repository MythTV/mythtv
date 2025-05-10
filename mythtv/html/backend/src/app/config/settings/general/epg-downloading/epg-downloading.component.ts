import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-epg-downloading',
  templateUrl: './epg-downloading.component.html',
  styleUrls: ['./epg-downloading.component.css']
})

export class EpgDownloadingComponent implements OnInit, AfterViewInit {

  @ViewChild("epgdownload")
  currentForm!: NgForm;

  successCount= 0;
  errorCount = 0;
  MythFillEnabled = true;
  MythFillDatabasePath = 'mythfilldatabase';
  MythFillDatabaseArgs = '';
  MythFillMinHour = 0;
  MythFillMaxHour = 23;
  MythFillGrabberSuggestsTime = true;

  constructor(public setupService: SetupService, private mythService: MythService) {
    this.getEpgDownload();
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  getEpgDownload() {
    this.successCount= 0;
    this.errorCount = 0;
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MythFillEnabled", Default: "1" })
      .subscribe({
        next: data => this.MythFillEnabled = (data.String == '1'),
        error: () => this.errorCount++
      });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MythFillDatabasePath", Default: "mythfilldatabase" })
      .subscribe({
        next: data => this.MythFillDatabasePath = data.String,
        error: () => this.errorCount++
      });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MythFillDatabaseArgs", Default: "" })
      .subscribe({
        next: data => this.MythFillDatabaseArgs = data.String,
        error: () => this.errorCount++
      });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MythFillMinHour", Default: "0" })
      .subscribe({
        next: data => this.MythFillMinHour = Number(data.String),
        error: () => this.errorCount++
      });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MythFillMaxHour", Default: "23" })
      .subscribe({
        next: data => this.MythFillMaxHour = Number(data.String),
        error: () => this.errorCount++
      });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MythFillGrabberSuggestsTime", Default: "1" })
      .subscribe({
        next: data => this.MythFillGrabberSuggestsTime = (data.String == '1'),
        error: () => this.errorCount++
      });
}

  EpgDownloadObs = {
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
      this.errorCount++
      if (this.currentForm)
        this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.successCount = 0;
    this.errorCount = 0;
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillEnabled",
      Value: this.MythFillEnabled ? "1" : "0"
    }).subscribe(this.EpgDownloadObs);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillDatabasePath",
      Value: this.MythFillDatabasePath
    }).subscribe(this.EpgDownloadObs);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillDatabaseArgs",
      Value: this.MythFillDatabaseArgs
    }).subscribe(this.EpgDownloadObs);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillMinHour",
      Value: String(this.MythFillMinHour)
    }).subscribe(this.EpgDownloadObs);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillMaxHour",
      Value: String(this.MythFillMaxHour)
    }).subscribe(this.EpgDownloadObs);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillGrabberSuggestsTime",
      Value: this.MythFillGrabberSuggestsTime ? "1" : "0"
    }).subscribe(this.EpgDownloadObs);
  }

}
