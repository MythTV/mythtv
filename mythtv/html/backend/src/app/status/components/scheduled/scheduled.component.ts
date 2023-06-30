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

  formatDate(date: string): string {
    if (!date)
      return '';
    if (date.length == 10)
      date = date + ' 00:00';
    return new Date(date).toLocaleDateString()
  }

  formatTime(date: string): string {
    if (!date)
      return '';
    // Get the locale specific time and remove the seconds
    const t = new Date(date);
    const tWithSecs = t.toLocaleTimeString() + ' ';
    return tWithSecs.replace(/:.. /, ' ');
  }

}
