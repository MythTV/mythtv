import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { ShutWake } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-shutdown-wakeup',
  templateUrl: './shutdown-wakeup.component.html',
  styleUrls: ['./shutdown-wakeup.component.css']
})
export class ShutdownWakeupComponent implements OnInit, AfterViewInit {

  shutwakeData: ShutWake = this.setupService.getShutWake();
  @ViewChild("shutwakeopt")
  currentForm!: NgForm;

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveShutWake(this.currentForm);
  }


}
