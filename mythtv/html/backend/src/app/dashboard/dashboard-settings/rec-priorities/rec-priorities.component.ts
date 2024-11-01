import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-rec-priorities',
  templateUrl: './rec-priorities.component.html',
  styleUrls: ['./rec-priorities.component.css']
})
export class RecPrioritiesComponent implements OnInit, AfterViewInit {

  @ViewChild("priorities")
  currentForm!: NgForm;

  btobList = [
    { Label: 'dashboard.priorities.btob_never', Value: 0 },
    { Label: 'dashboard.priorities.btob_diff', Value: 1 },
    { Label: 'dashboard.priorities.btob_always', Value: 2 },
  ];

  successCount = 0;
  errorCount = 0;

  SchedOpenEnd = 0;
  PrefInputPriority = 2;
  HDTVRecPriority = 0;
  WSRecPriority = 0;
  SignLangRecPriority = 0;
  OnScrSubRecPriority = 0;
  CCRecPriority = 0;
  HardHearRecPriority = 0;
  AudioDescRecPriority = 0;


  constructor(private mythService: MythService, private translate: TranslateService,
    private setupService: SetupService) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadValues();
    this.markPristine();
  }

  loadTranslations() {
    this.btobList.forEach((entry) =>
      this.translate.get(entry.Label).subscribe(data => entry.Label = data));
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  loadValues() {

    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "SchedOpenEnd", Default: "0" })
      .subscribe({
        next: data => this.SchedOpenEnd = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "PrefInputPriority", Default: "2" })
      .subscribe({
        next: data => this.PrefInputPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "HDTVRecPriority", Default: "0" })
      .subscribe({
        next: data => this.HDTVRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WSRecPriority", Default: "0" })
      .subscribe({
        next: data => this.WSRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "SignLangRecPriority", Default: "0" })
      .subscribe({
        next: data => this.SignLangRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "OnScrSubRecPriority", Default: "0" })
      .subscribe({
        next: data => this.OnScrSubRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "CCRecPriority", Default: "0" })
      .subscribe({
        next: data => this.CCRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "HardHearRecPriority", Default: "0" })
      .subscribe({
        next: data => this.HardHearRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AudioDescRecPriority", Default: "0" })
      .subscribe({
        next: data => this.AudioDescRecPriority = Number(data.String),
        error: () => this.errorCount++
      });
  }

  markPristine() {
    setTimeout(() => this.currentForm.form.markAsPristine(), 200);
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
      HostName: '_GLOBAL_', Key: "SchedOpenEnd",
      Value: String(this.SchedOpenEnd)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "PrefInputPriority",
      Value: String(this.PrefInputPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "HDTVRecPriority",
      Value: String(this.HDTVRecPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "WSRecPriority",
      Value: String(this.WSRecPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "SignLangRecPriority",
      Value: String(this.SignLangRecPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "OnScrSubRecPriority",
      Value: String(this.OnScrSubRecPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "CCRecPriority",
      Value: String(this.CCRecPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "HardHearRecPriority",
      Value: String(this.HardHearRecPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "AudioDescRecPriority",
      Value: String(this.AudioDescRecPriority)
    }).subscribe(this.swObserver);
  }

}
