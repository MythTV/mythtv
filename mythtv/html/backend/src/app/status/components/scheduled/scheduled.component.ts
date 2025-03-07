import { Component, OnInit, Input } from '@angular/core';
import { ScheduleOrProgram } from 'src/app/services/interfaces/program.interface';
import { UtilityService } from 'src/app/services/utility.service';
import { NgIf, NgFor } from '@angular/common';
import { Tooltip } from 'primeng/tooltip';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-status-scheduled',
    templateUrl: './scheduled.component.html',
    styleUrls: ['./scheduled.component.css', '../../status.component.css'],
    standalone: true,
    imports: [NgIf, NgFor, Tooltip, TranslatePipe]
})
export class ScheduledComponent implements OnInit {
  @Input() scheduled?: ScheduleOrProgram[];

  constructor(public utility: UtilityService) { }

  ngOnInit(): void {
  }


}
