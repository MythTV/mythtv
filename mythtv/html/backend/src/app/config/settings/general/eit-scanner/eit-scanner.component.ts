import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-eit-scanner',
  templateUrl: './eit-scanner.component.html',
  styleUrls: ['./eit-scanner.component.css']
})
export class EitScannerComponent implements OnInit, AfterViewInit {

  @ViewChild("eitscanopt")
  currentForm!: NgForm;

  successCount = 0;
  errorCount = 0;
  EITTransportTimeout = 5;
  EITCrawIdleStart = 60;
  EITScanPeriod = 15;

  constructor(public setupService: SetupService, private mythService: MythService) {
    this.getEITScanner();
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  getEITScanner(){

    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "EITTransportTimeout", Default: "5" })
      .subscribe({
        next: data => this.EITTransportTimeout = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "EITCrawIdleStart", Default: "60" })
      .subscribe({
        next: data => this.EITCrawIdleStart = Number(data.String),
        error: () => this.errorCount++
      });
      this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "EITScanPeriod", Default: "15" })
      .subscribe({
        next: data => this.EITScanPeriod = Number(data.String),
        error: () => this.errorCount++
      });

  }

  eitObserver = {
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
      HostName: '_GLOBAL_', Key: "EITTransportTimeout",
      Value: String(this.EITTransportTimeout)
    }).subscribe(this.eitObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "EITCrawIdleStart",
      Value: String(this.EITCrawIdleStart)
    }).subscribe(this.eitObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "EITScanPeriod",
      Value: String(this.EITScanPeriod)
    }).subscribe(this.eitObserver);
  }

}
