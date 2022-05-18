import { Component, OnInit } from '@angular/core';

import { EITScanner } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-eit-scanner',
  templateUrl: './eit-scanner.component.html',
  styleUrls: ['./eit-scanner.component.css']
})
export class EitScannerComponent implements OnInit {

  eitData: EITScanner = this.setupService.getEITScanner();

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveEITScanner();
  }

}
