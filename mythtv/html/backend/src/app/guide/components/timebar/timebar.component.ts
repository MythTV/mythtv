import { Component, OnInit, Input } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

@Component({
  selector: 'app-guide-timebar',
  templateUrl: './timebar.component.html',
  styleUrls: ['./timebar.component.css']
})
export class TimebarComponent implements OnInit {
  @Input() startTime! : string;

  constructor(private translate : TranslateService) { }

  ngOnInit(): void {
  }

  toLocalShortTime(date : Date) : string {
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

  segmentToStartTime(segment : number) {
      let st = new Date(this.startTime);
      const offset = segment * 1800000; /* 30 mins */
      const t = new Date(st.getTime() + offset);
      return this.toLocalShortTime(t);
  }
}
