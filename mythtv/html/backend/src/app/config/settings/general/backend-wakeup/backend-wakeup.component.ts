import { Component, OnInit } from '@angular/core';

import { BackendWake } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-backend-wakeup',
  templateUrl: './backend-wakeup.component.html',
  styleUrls: ['./backend-wakeup.component.css']
})
export class BackendWakeupComponent implements OnInit {

  beWakeData: BackendWake = this.setupService.getBackendWake();

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveBackendWake();
  }


}
