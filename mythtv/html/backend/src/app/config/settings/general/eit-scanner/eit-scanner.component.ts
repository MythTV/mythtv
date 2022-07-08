import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { EITScanner } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-eit-scanner',
  templateUrl: './eit-scanner.component.html',
  styleUrls: ['./eit-scanner.component.css']
})
export class EitScannerComponent implements OnInit,AfterViewInit {

  eitData: EITScanner = this.setupService.getEITScanner();
  @ViewChild("eitscanopt")
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
    this.setupService.saveEITScanner(this.currentForm);
  }

}
