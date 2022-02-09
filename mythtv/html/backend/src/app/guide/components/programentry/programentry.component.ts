import { Component, Input, OnInit } from '@angular/core';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';

@Component({
  selector: 'app-guide-programentry',
  templateUrl: './programentry.component.html',
  styleUrls: ['./programentry.component.css']
})
export class ProgramEntryComponent implements OnInit {
  @Input() program! :         ScheduleOrProgram;
  @Input() guideStartTime! :  string;
  @Input() guideEndTime! :    string;
  displayDetails :            boolean = false;

  constructor() { }

  ngOnInit(): void {
  }

  durationToWidth() : number {
    let p_start = new Date(this.program.StartTime);
    let p_end   = new Date(this.program.EndTime);
    let w_start = new Date(this.guideStartTime);
    let w_end   = new Date(this.guideEndTime);
    let beginsBefore = p_start < w_start;
    let endsAfter = p_end > w_end;
    let startPoint = (beginsBefore ? w_start : p_start);
    let endPoint = (endsAfter ? w_end: p_end);
    let timeWindow = w_end.getTime() - w_start.getTime();
    let programWindow = endPoint.getTime() - startPoint.getTime();
    let program_width = ((programWindow / timeWindow)*100);
    return program_width;
  }

  // Taken from timebar.components.ts, but takes string arg, not Date. TODO: Refactor
  toLocalShortTime(date : string) : string {
    let d = new Date(date);
    let ampm = 'AM';
    let h;
    let m = d.getMinutes().toString().padStart(2, '0');
    let hour = d.getHours();
    if (hour == 0) {
        h = 12;
    } else if (hour > 12) {
        h = hour - 12;
    } else {
        h = hour;
    }
    if (hour >= 12) {
        ampm = 'PM';
    }
    return h + ":" + m + " " + ampm;
  }

  showDetailsDialog() {
    this.displayDetails = true;
  }
}
