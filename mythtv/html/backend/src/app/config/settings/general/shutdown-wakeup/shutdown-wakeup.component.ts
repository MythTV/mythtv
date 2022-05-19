import { Component, OnInit } from '@angular/core';

import { ShutWake } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-shutdown-wakeup',
  templateUrl: './shutdown-wakeup.component.html',
  styleUrls: ['./shutdown-wakeup.component.css']
})
export class ShutdownWakeupComponent implements OnInit {

  shutwakeData: ShutWake = this.setupService.getShutWake();

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveShutWake();
  }


}
