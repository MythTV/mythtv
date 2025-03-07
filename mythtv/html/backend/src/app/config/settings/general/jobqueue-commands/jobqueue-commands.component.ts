import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm, FormsModule } from '@angular/forms';

import { JobQCommands } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';
import { Card } from 'primeng/card';
import { PrimeTemplate } from 'primeng/api';
import { NgFor, NgIf } from '@angular/common';
import { TranslateDirective, TranslatePipe } from '@ngx-translate/core';
import { Message } from 'primeng/message';
import { Button } from 'primeng/button';

@Component({
    selector: 'app-jobqueue-commands',
    templateUrl: './jobqueue-commands.component.html',
    styleUrls: ['./jobqueue-commands.component.css'],
    standalone: true,
    imports: [FormsModule, Card, PrimeTemplate, NgFor, TranslateDirective, NgIf, Message, Button, TranslatePipe]
})
export class JobqueueCommandsComponent implements OnInit, AfterViewInit {

  @ViewChild("jobqcommands")
  currentForm!: NgForm;

  JobQCommandsData!: JobQCommands;
  items: number[] = [0, 1, 2, 3];

  constructor(public setupService: SetupService) {
    this.JobQCommandsData = this.setupService.getJobQCommands();
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  saveForm() {
    this.setupService.saveJobQCommands(this.currentForm);
  }

}
