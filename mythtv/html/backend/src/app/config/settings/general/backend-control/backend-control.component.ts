import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm, FormsModule } from '@angular/forms';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';
import { Card } from 'primeng/card';
import { PrimeTemplate } from 'primeng/api';
import { NgIf } from '@angular/common';
import { Message } from 'primeng/message';
import { Button } from 'primeng/button';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-backend-control',
    templateUrl: './backend-control.component.html',
    styleUrls: ['./backend-control.component.css'],
    standalone: true,
    imports: [FormsModule, Card, PrimeTemplate, NgIf, Message, Button, TranslatePipe]
})
export class BackendControlComponent implements OnInit, AfterViewInit {

  @ViewChild("backendcontrol")
  currentForm!: NgForm;

  successCount = 0;
  errorCount = 0;
  BackendStopCommand = "killall mythbackend";
  BackendStartCommand = "mythbackend";

  constructor(public setupService: SetupService, private mythService: MythService) { }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  getBackendControl() {
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "BackendStopCommand", Default: "killall mythbackend" })
      .subscribe({
        next: data => this.BackendStopCommand = data.String,
        error: () => this.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "BackendStartCommand", Default: "mythbackend" })
      .subscribe({
        next: data => this.BackendStartCommand = data.String,
        error: () => this.errorCount++
      });
  }

  becObserver = {
    next: (x: any) => {
      if (x.bool)
        this.successCount++;
      else {
        this.errorCount++;
        if (this.currentForm)
          this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      if (this.currentForm)
        this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.successCount = 0;
    this.errorCount = 0;
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "BackendStopCommand",
      Value: this.BackendStopCommand
    }).subscribe(this.becObserver);
    this.mythService.PutSetting({
      HostName: '_GLOBAL_', Key: "BackendStartCommand",
      Value: this.BackendStartCommand
    }).subscribe(this.becObserver);
  }

}
