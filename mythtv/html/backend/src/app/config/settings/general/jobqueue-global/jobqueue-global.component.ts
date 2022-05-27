import { Component, OnInit } from '@angular/core';

import { JobQGlobal } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-jobqueue-global',
  templateUrl: './jobqueue-global.component.html',
  styleUrls: ['./jobqueue-global.component.css']
})
export class JobqueueGlobalComponent implements OnInit {

  JobQGlobalData: JobQGlobal = this.setupService.getJobQGlobal();

  constructor(private setupService: SetupService) {
  }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveJobQGlobal();
  }

}
