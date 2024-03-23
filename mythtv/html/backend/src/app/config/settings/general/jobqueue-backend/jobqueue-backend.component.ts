import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { Calendar } from 'primeng/calendar';
import { TranslateService } from '@ngx-translate/core';

import { JobQCommands } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';
import { NgForm } from '@angular/forms';
import { GetSettingResponse } from 'src/app/services/interfaces/myth.interface';
import { Observable } from 'rxjs';
import { MythService } from 'src/app/services/myth.service';

interface ddParm {
  name: string,
  code: string
}

@Component({
  selector: 'app-jobqueue-backend',
  templateUrl: './jobqueue-backend.component.html',
  styleUrls: ['./jobqueue-backend.component.css']
})

export class JobqueueBackendComponent implements OnInit, AfterViewInit {

  JobQCmds!: JobQCommands;

  @ViewChild("jobqbackend")
  currentForm!: NgForm;

  successCount = 0;
  errorCount = 0;
  hostName = '';
  JobQueueMaxSimultaneousJobs = 1;
  JobQueueCheckFrequency = 60;
  JobQueueWindowStart = new Date(0);
  @ViewChild("JobQueueWindowStartHT") JobQueueWindowStartHT!: Calendar;
  JobQueueWindowStartHT$ = new Observable<GetSettingResponse>();
  JobQueueWindowEnd = new Date(0);
  @ViewChild("JobQueueWindowEndHT") JobQueueWindowEndHT!: Calendar;
  JobQueueWindowEndHT$ = new Observable<GetSettingResponse>();
  JobQueueCPU = "0";
  JobAllowMetadata = true;
  JobAllowCommFlag = true;
  JobAllowTranscode = true;
  JobAllowPreview = true;
  JobAllowUserJob1 = false;
  JobAllowUserJob2 = false;
  JobAllowUserJob3 = false;
  JobAllowUserJob4 = false;



  cpuOptions: ddParm[] = [
    { name: "settings.jobqbackend.cpu_low", code: "0" },
    { name: "settings.jobqbackend.cpu_med", code: "1" },
    { name: "settings.jobqbackend.cpu_high", code: "2" }
  ];

  constructor(public setupService: SetupService, private translate: TranslateService,
    private mythService: MythService) {
    this.getJobQBackend();

    // These two calls are a work-around for the bug that
    // the time-picker does not update when the backend value
    // is filled into the Date backing field.
    this.JobQueueWindowStartHT$.subscribe
      ({ complete: () => this.JobQueueWindowStartHT.updateInputfield() });
    this.JobQueueWindowEndHT$.subscribe
      ({ complete: () => this.JobQueueWindowEndHT.updateInputfield() });

    this.JobQCmds = this.setupService.getJobQCommands();

    translate.get(this.cpuOptions[0].name).subscribe(data => this.cpuOptions[0].name = data);
    translate.get(this.cpuOptions[1].name).subscribe(data => this.cpuOptions[1].name = data);
    translate.get(this.cpuOptions[2].name).subscribe(data => this.cpuOptions[2].name = data);
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  getJobQBackend() {
    this.successCount = 0;
    this.errorCount = 0
    this.setupService.parseTime(this.JobQueueWindowStart, "00:00");
    this.setupService.parseTime(this.JobQueueWindowEnd, "23:59");
    this.mythService.GetHostName().subscribe({
      next: data => {
        this.hostName = data.String;
        this.getSettings();
      },
      error: () => this.errorCount++
    })

  }

  getSettings() {
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobQueueMaxSimultaneousJobs", Default: "1" })
      .subscribe({
        next: data => this.JobQueueMaxSimultaneousJobs = Number(data.String),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobQueueCheckFrequency", Default: "60" })
      .subscribe({
        next: data => this.JobQueueCheckFrequency = Number(data.String),
        error: () => this.errorCount++
      });
    this.JobQueueWindowStartHT$ = this.mythService.GetSetting({
      HostName: this.hostName, Key: "JobQueueWindowStart", Default: "00:00"
    });
    this.JobQueueWindowStartHT$
      .subscribe({
        next: data => {
          this.setupService.parseTime(this.JobQueueWindowStart, data.String);
          this.JobQueueWindowStartHT.updateInputfield();
        },
        error: () => this.errorCount++
      });
    this.JobQueueWindowEndHT$ = this.mythService.GetSetting({
      HostName: this.hostName, Key: "JobQueueWindowEnd", Default: "23:59"
    });
    this.JobQueueWindowEndHT$
      .subscribe({
        next: data => {
          this.setupService.parseTime(this.JobQueueWindowEnd, data.String);
          this.JobQueueWindowEndHT.updateInputfield();
        },
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobQueueCPU", Default: "0" })
      .subscribe({
        next: data => this.JobQueueCPU = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowMetadata", Default: "1" })
      .subscribe({
        next: data => this.JobAllowMetadata = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowCommFlag", Default: "1" })
      .subscribe({
        next: data => this.JobAllowCommFlag = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowTranscode", Default: "1" })
      .subscribe({
        next: data => this.JobAllowTranscode = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowPreview", Default: "1" })
      .subscribe({
        next: data => this.JobAllowPreview = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowUserJob1", Default: "0" })
      .subscribe({
        next: data => this.JobAllowUserJob1 = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowUserJob2", Default: "0" })
      .subscribe({
        next: data => this.JobAllowUserJob2 = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowUserJob3", Default: "0" })
      .subscribe({
        next: data => this.JobAllowUserJob3 = (data.String == '1'),
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: this.hostName, Key: "JobAllowUserJob4", Default: "0" })
      .subscribe({
        next: data => this.JobAllowUserJob4 = (data.String == '1'),
        error: () => this.errorCount++
      });
  }

  jqbObserver = {
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
      HostName: this.hostName, Key: "JobQueueMaxSimultaneousJobs",
      Value: String(this.JobQueueMaxSimultaneousJobs)
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobQueueCheckFrequency",
      Value: String(this.JobQueueCheckFrequency)
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobQueueWindowStart",
      Value: this.setupService.formatTime(this.JobQueueWindowStart)
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobQueueWindowEnd",
      Value: this.setupService.formatTime(this.JobQueueWindowEnd)
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobQueueCPU",
      Value: this.JobQueueCPU
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowMetadata",
      Value: this.JobAllowMetadata ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowCommFlag",
      Value: this.JobAllowCommFlag ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowTranscode",
      Value: this.JobAllowTranscode ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowPreview",
      Value: this.JobAllowPreview ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowUserJob1",
      Value: this.JobAllowUserJob1 ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowUserJob2",
      Value: this.JobAllowUserJob2 ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowUserJob3",
      Value: this.JobAllowUserJob3 ? "1" : "0"
    }).subscribe(this.jqbObserver);
    this.mythService.PutSetting({
      HostName: this.hostName, Key: "JobAllowUserJob4",
      Value: this.JobAllowUserJob4 ? "1" : "0"
    }).subscribe(this.jqbObserver);

  }

}
