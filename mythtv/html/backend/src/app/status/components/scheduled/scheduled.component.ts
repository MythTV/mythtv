import { Component, OnInit, Input } from '@angular/core';
import { MatTooltipModule, TooltipPosition } from '@angular/material/tooltip';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';

@Component({
  selector: 'app-status-scheduled',
  templateUrl: './scheduled.component.html',
  styleUrls: ['./scheduled.component.css', '../../status.component.css']
})
export class ScheduledComponent implements OnInit {
  @Input() scheduled?: ScheduleOrProgram[];

  constructor() { }

  ngOnInit(): void {
  }

}
