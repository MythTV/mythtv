import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-auto-expire',
  templateUrl: './auto-expire.component.html',
  styleUrls: ['./auto-expire.component.css']
})
export class AutoExpireComponent implements OnInit, AfterViewInit {

  @ViewChild("autoexpire")
  currentForm!: NgForm;


  methodList = [
    { Label: 'dashboard.autoexpire.method_oldest', Value: 1 },
    { Label: 'dashboard.autoexpire.method_lowest', Value: 2 },
    { Label: 'dashboard.autoexpire.method_weighted', Value: 3 },
  ];

  successCount = 0;
  errorCount = 0;

  AutoExpireMethod = 1;
  RerecordWatched = false;
  AutoExpireWatchedPriority = false;
  AutoExpireLiveTVMaxAge = 1;
  AutoExpireDayPriority = 3;
  AutoExpireExtraSpace = 1;
  DeletedMaxAge = 0;

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
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoExpireMethod", Default: "1" })
      .subscribe({
        next: data => this.AutoExpireMethod = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "RerecordWatched", Default: "0" })
      .subscribe({
        next: data => this.RerecordWatched = (data.String == "1"),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoExpireWatchedPriority", Default: "0" })
      .subscribe({
        next: data => this.AutoExpireWatchedPriority = (data.String == "1"),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoExpireLiveTVMaxAge", Default: "1" })
      .subscribe({
        next: data => this.AutoExpireLiveTVMaxAge = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoExpireDayPriority", Default: "3" })
      .subscribe({
        next: data => this.AutoExpireDayPriority = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoExpireExtraSpace", Default: "1" })
      .subscribe({
        next: data => this.AutoExpireExtraSpace = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DeletedMaxAge", Default: "0" })
      .subscribe({
        next: data => this.DeletedMaxAge = Number(data.String),
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
      HostName: '_GLOBAL_', Key: "AutoExpireMethod",
      Value: String(this.AutoExpireMethod)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "RerecordWatched",
      Value: this.RerecordWatched ? "1" : "0"
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "AutoExpireWatchedPriority",
      Value: this.AutoExpireWatchedPriority ? "1" : "0"
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "AutoExpireLiveTVMaxAge",
      Value: String(this.AutoExpireLiveTVMaxAge)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "AutoExpireDayPriority",
      Value: String(this.AutoExpireDayPriority)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "AutoExpireExtraSpace",
      Value: String(this.AutoExpireExtraSpace)
    }).subscribe(this.swObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "DeletedMaxAge",
      Value: String(this.DeletedMaxAge)
    }).subscribe(this.swObserver);
  }

}
