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
    console.log("save form clicked");
    this.successCount = 0;
    this.errorCount = 0;
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "MythFillEnabled",
      Value: this.MythFillEnabled ? "1" : "0"
    }).subscribe(this.EpgDownloadObs);
  }

}
