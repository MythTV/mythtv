import { Component, OnInit } from '@angular/core';

import { JobQCommands } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-jobqueue-commands',
  templateUrl: './jobqueue-commands.component.html',
  styleUrls: ['./jobqueue-commands.component.css']
})
export class JobqueueCommandsComponent implements OnInit {

  JobQCommandsData!: JobQCommands;
  items: number [] = [0,1,2,3];

  constructor(private setupService: SetupService) {
    this.JobQCommandsData = this.setupService.getJobQCommands();
  }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveJobQCommands();
  }

}
