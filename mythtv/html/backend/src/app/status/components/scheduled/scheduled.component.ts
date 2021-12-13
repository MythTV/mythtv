import { Component, OnInit, Input } from '@angular/core';
import { ScheduleOrProgram } from 'src/app/services/interfaces/schedule.interface';

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
