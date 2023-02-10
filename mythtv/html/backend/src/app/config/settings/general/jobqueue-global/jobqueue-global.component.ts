import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { JobQGlobal } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-jobqueue-global',
  templateUrl: './jobqueue-global.component.html',
  styleUrls: ['./jobqueue-global.component.css']
})
export class JobqueueGlobalComponent implements OnInit, AfterViewInit {

  JobQGlobalData: JobQGlobal = this.setupService.getJobQGlobal();
  @ViewChild("jobqglobal")
  currentForm!: NgForm;

  constructor(private setupService: SetupService) {
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveJobQGlobal(this.currentForm);
  }

}
