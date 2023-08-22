import { Component, OnInit, Input } from '@angular/core';
import { JobQueueJob } from 'src/app/services/interfaces/jobqueue.interface';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-status-jobqueue',
  templateUrl: './jobqueue.component.html',
  styleUrls: ['./jobqueue.component.css', '../../status.component.css']
})
export class JobqueueComponent implements OnInit {
  @Input() jobqueue?: JobQueueJob[];

  constructor(public utility: UtilityService) { }

  ngOnInit(): void {
  }

}
