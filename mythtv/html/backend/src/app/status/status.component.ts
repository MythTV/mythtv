import { Component, OnInit } from '@angular/core';
import { BackendStatusResponse } from '../services/interfaces/status.interface';
import { StatusService } from '../services/status.service';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';

@Component({
  selector: 'app-status',
  templateUrl: './status.component.html',
  styleUrls: ['./status.component.css']
})
export class StatusComponent implements OnInit {
  m_status$!: Observable<BackendStatusResponse>;

  constructor(private statusService: StatusService) { }

  ngOnInit(): void {
    this.m_status$ = this.statusService.GetBackendStatus();
  }
}
