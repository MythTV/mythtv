import { Component, OnInit, Input } from '@angular/core';
import { JobQueueJob } from 'src/app/services/interfaces/jobqueue.interface';
import { UtilityService } from 'src/app/services/utility.service';
import { NgIf, NgFor } from '@angular/common';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-status-jobqueue',
    templateUrl: './jobqueue.component.html',
    styleUrls: ['./jobqueue.component.css', '../../status.component.css'],
    standalone: true,
    imports: [NgIf, NgFor, TranslatePipe]
})
export class JobqueueComponent implements OnInit {
  @Input() jobqueue?: JobQueueJob[];

  constructor(public utility: UtilityService) { }

  ngOnInit(): void {
  }

}
