import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-jobs',
  templateUrl: './jobs.component.html',
  styleUrls: ['./jobs.component.css']
})
export class JobsComponent implements OnInit, AfterViewInit {

  @ViewChild("jobs")
  currentForm!: NgForm;


  // COMM_DETECT_BLANK       = 0x00000001,
  // COMM_DETECT_SCENE       = 0x00000002,
  // COMM_DETECT_LOGO        = 0x00000004,
  // COMM_DETECT_2           = 0x00000100, (Experimental)
  // COMM_DETECT_PREPOSTROLL = 0x00000200,


  methodList = [
    { Label: 'dashboard.jobs.method_all', Value: 7 },
    { Label: 'dashboard.jobs.method_blank', Value: 1 },
    { Label: 'dashboard.jobs.method_blank_scene', Value: 3 },
    { Label: 'dashboard.jobs.method_scene', Value: 2 },
    { Label: 'dashboard.jobs.method_logo', Value: 4 },
    { Label: 'dashboard.jobs.method_exp_blank_logo', Value: 261 },
    { Label: 'dashboard.jobs.method_roll_blank_scene', Value: 515 },
  ];

  successCount = 0;
  errorCount = 0;

  CommercialSkipMethod = 7;
  CommFlagFast = false;
  AggressiveCommDetect = true;
  DeferAutoTranscodeDays = 0;

  constructor(private mythService: MythService, private translate: TranslateService,
    private setupService: SetupService) {
  }

  ngOnInit(): void {
    this.loadTranslations();
    this.loadValues();
    this.markPristine();
  }

  loadTranslations() {
    this.methodList.forEach((entry) =>
      this.translate.get(entry.Label).subscribe(data => entry.Label = data));
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  loadValues() {
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "CommercialSkipMethod", Default: "7" })
      .subscribe({
        next: data => this.CommercialSkipMethod = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "CommFlagFast", Default: "0" })
      .subscribe({
        next: data => this.CommFlagFast = (data.String == "1"),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AggressiveCommDetect", Default: "0" })
      .subscribe({
        next: data => this.AggressiveCommDetect = (data.String == "1"),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DeferAutoTranscodeDays", Default: "7" })
      .subscribe({
        next: data => this.DeferAutoTranscodeDays = Number(data.String),
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
      HostName: '_GLOBAL_', Key: "CommercialSkipMethod",
      Value: String(this.CommercialSkipMethod)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "CommFlagFast",
      Value: this.CommFlagFast ? "1" : "0"
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "AggressiveCommDetect",
      Value: this.AggressiveCommDetect ? "1" : "0"
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "DeferAutoTranscodeDays",
      Value: String(this.DeferAutoTranscodeDays)
    }).subscribe(this.swObserver);

  }

}
