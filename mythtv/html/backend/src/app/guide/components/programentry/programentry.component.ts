import { Component, Input, OnInit } from '@angular/core';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { GuideComponent } from '../../guide.component';
import { DataService } from 'src/app/services/data.service';
import { Channel } from 'src/app/services/interfaces/channel.interface';

@Component({
  selector: 'app-guide-programentry',
  templateUrl: './programentry.component.html',
  styleUrls: ['./programentry.component.css']
})
export class ProgramEntryComponent implements OnInit {
  @Input() program!: ScheduleOrProgram;
  @Input() channel!: Channel;
  @Input() guideStartTime!: string;
  @Input() guideEndTime!: string;
  @Input() guideComponent!: GuideComponent;

  editSchedule: boolean = false;
  typeclass = '';
  catclass = ''
  regex = /[^a-z0-9]/g;

  constructor(public dataService: DataService) {
  }

  ngOnInit(): void {
    this.typeclass = 'guide_type_' + this.program.CatType;
    this.catclass = 'guide_cat_'
      + this.program.Category.toLowerCase().replace(this.regex, '_');
  }

  durationToWidth(): number {
    let p_start = new Date(this.program.StartTime);
    let p_end = new Date(this.program.EndTime);
    let w_start = new Date(this.guideStartTime);
    let w_end = new Date(this.guideEndTime);
    let beginsBefore = p_start < w_start;
    let endsAfter = p_end > w_end;
    let startPoint = (beginsBefore ? w_start : p_start);
    let endPoint = (endsAfter ? w_end : p_end);
    let timeWindow = w_end.getTime() - w_start.getTime();
    let programWindow = endPoint.getTime() - startPoint.getTime();
    let program_width = ((programWindow / timeWindow) * 100);
    return program_width;
  }

  openDialog() {
    if (this.guideComponent.inter.sched)
      this.guideComponent.inter.sched.open(this.program, this.channel);
  }
}
