import { Component, OnInit, ViewChild } from '@angular/core';
import { Calendar } from 'primeng/calendar';
import {TranslateService} from '@ngx-translate/core';

import { JobQBackend, JobQCommands } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

interface ddParm {
  name: string,
  code: string
}

@Component({
  selector: 'app-jobqueue-backend',
  templateUrl: './jobqueue-backend.component.html',
  styleUrls: ['./jobqueue-backend.component.css']
})

export class JobqueueBackendComponent implements OnInit {

  JobQBData: JobQBackend;
  JobQCmds!: JobQCommands;

  @ViewChild("JobQueueWindowStart")
  JobQueueWindowStart! : Calendar;

  @ViewChild("JobQueueWindowEnd")
  JobQueueWindowEnd! : Calendar;


  cpuOptions: ddParm[] = [
    {name: "settings.jobqbackend.cpu_low", code: "0"},
    {name: "settings.jobqbackend.cpu_med", code: "1"},
    {name: "settings.jobqbackend.cpu_high", code: "2"}
  ];

  constructor(private setupService: SetupService, private translate: TranslateService) {
    this.JobQBData = this.setupService.getJobQBackend();
    // These two calls are a work-around for the bug that
    // the time-picker does not update when the backend value
    // is filled into the Date backing field.
    this.JobQBData.JobQueueWindowStart$.subscribe
      ({complete: () => this.JobQueueWindowStart.updateInputfield()});
    this.JobQBData.JobQueueWindowEnd$.subscribe
      ({complete: () => this.JobQueueWindowEnd.updateInputfield()});

    this.JobQCmds = this.setupService.getJobQCommands();

    translate.get(this.cpuOptions[0].name).subscribe(data => this.cpuOptions[0].name = data);
    translate.get(this.cpuOptions[1].name).subscribe(data => this.cpuOptions[1].name = data);
    translate.get(this.cpuOptions[2].name).subscribe(data => this.cpuOptions[2].name = data);
  }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveJobQBackend();
  }

}
