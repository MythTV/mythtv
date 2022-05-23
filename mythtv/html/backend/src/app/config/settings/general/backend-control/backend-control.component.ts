import { Component, OnInit } from '@angular/core';

import { BackendControl } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-backend-control',
  templateUrl: './backend-control.component.html',
  styleUrls: ['./backend-control.component.css']
})
export class BackendControlComponent implements OnInit {

  beCtrlData: BackendControl = this.setupService.getBackendControl();

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveBackendControl();
  }

}
