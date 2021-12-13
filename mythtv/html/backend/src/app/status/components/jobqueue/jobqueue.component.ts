import { Component, OnInit, Input } from '@angular/core';
import { JobQueueJob } from 'src/app/services/interfaces/jobqueue.interface';

@Component({
  selector: 'app-status-jobqueue',
  templateUrl: './jobqueue.component.html',
  styleUrls: ['./jobqueue.component.css', '../../status.component.css']
})
export class JobqueueComponent implements OnInit {
  @Input() jobqueue?: JobQueueJob[];

  constructor() { }

  ngOnInit(): void {
  }

  JobStatusText(jobstatus: number) : string {
    switch(jobstatus) {
      case 0x0001:
        return "Queued";
      case 0x0002:
        return "Pending";
      case 0x0003:
        return "Starting";
      case 0x0004:
        return "Running";
      case 0x0005:
        return "Stopping";
      case 0x0006:
        return "Paused";
      case 0x0007:
        return "Retry";
      case 0x0008:
        return "Erroring";
      case 0x0009:
        return "Aborting";
      case 0x0100:
        return "Done (Invalid Status!)";
      case 0x0110:
        return "Finished";
      case 0x0120:
        return "Aborted";
      case 0x0130:
        return "Errored";
      case 0x0140:
        return "Cancelled";
      default:
        return "Unknown";
    }
  }

  JobTypeText(jobtype: number) : string {
    switch(jobtype) {
      case 0x0001:
        return "Transcode";
      case 0x0002:
        return "Commflag";
      case 0x0004:
        return "Lookup Metadata";
      case 0x0008:
        return "Preview Generation";
      default:
        return "Unknown";
    }
  }
}
