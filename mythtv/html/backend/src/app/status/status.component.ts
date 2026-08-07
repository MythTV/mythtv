import { Component, OnInit } from '@angular/core';
import { BackendStatusResponse } from '../services/interfaces/status.interface';
import { StatusService } from '../services/status.service';
import { Observable } from 'rxjs';
import { TranslatePipe } from '@ngx-translate/core';
import { FrontendsComponent } from './components/frontends/frontends.component';
import { BackendsComponent } from './components/backends/backends.component';
import { MachineinfoComponent } from './components/machineinfo/machineinfo.component';
import { JobqueueComponent } from './components/jobqueue/jobqueue.component';
import { ScheduledComponent } from './components/scheduled/scheduled.component';
import { EncodersComponent } from './components/encoders/encoders.component';
import { AsyncPipe } from '@angular/common';
import { DbBackupsComponent } from './components/db-backups/db-backups.component';

@Component({
    selector: 'app-status',
    templateUrl: './status.component.html',
    styleUrls: ['./status.component.css'],
    imports: [EncodersComponent, ScheduledComponent, JobqueueComponent, MachineinfoComponent,
         DbBackupsComponent, BackendsComponent, FrontendsComponent, AsyncPipe, TranslatePipe]
})
export class StatusComponent implements OnInit {
    m_status$!: Observable<BackendStatusResponse>;
    status?: BackendStatusResponse;

    constructor(private statusService: StatusService) { }

    ngOnInit(): void {
        this.m_status$ = this.statusService.GetBackendStatus();
    }
}
