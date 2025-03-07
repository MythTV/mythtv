import { Component, OnInit } from '@angular/core';
import { BackendStatusResponse } from '../services/interfaces/status.interface';
import { StatusService } from '../services/status.service';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';
import { NgIf, AsyncPipe } from '@angular/common';
import { EncodersComponent } from './components/encoders/encoders.component';
import { ScheduledComponent } from './components/scheduled/scheduled.component';
import { JobqueueComponent } from './components/jobqueue/jobqueue.component';
import { MachineinfoComponent } from './components/machineinfo/machineinfo.component';
import { BackendsComponent } from './components/backends/backends.component';
import { FrontendsComponent } from './components/frontends/frontends.component';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-status',
    templateUrl: './status.component.html',
    styleUrls: ['./status.component.css'],
    standalone: true,
    imports: [NgIf, EncodersComponent, ScheduledComponent, JobqueueComponent, MachineinfoComponent, BackendsComponent, FrontendsComponent, AsyncPipe, TranslatePipe]
})
export class StatusComponent implements OnInit {
  m_status$!: Observable<BackendStatusResponse>;

  constructor(private statusService: StatusService) { }

  ngOnInit(): void {
    this.m_status$ = this.statusService.GetBackendStatus();
  }
}
