import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { BackendWake } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-backend-wakeup',
  templateUrl: './backend-wakeup.component.html',
  styleUrls: ['./backend-wakeup.component.css']
})
export class BackendWakeupComponent implements OnInit, AfterViewInit {

  beWakeData: BackendWake = this.setupService.getBackendWake();
  @ViewChild("backendwakeup")
  currentForm!: NgForm;

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveBackendWake(this.currentForm);
  }


}
