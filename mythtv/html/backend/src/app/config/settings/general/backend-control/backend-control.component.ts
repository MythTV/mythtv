import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { BackendControl } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-backend-control',
  templateUrl: './backend-control.component.html',
  styleUrls: ['./backend-control.component.css']
})
export class BackendControlComponent implements OnInit, AfterViewInit {

  beCtrlData: BackendControl = this.setupService.getBackendControl();
  @ViewChild("backendcontrol")
  currentForm!: NgForm;

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }
  
  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveBackendControl(this.currentForm);
  }

}
