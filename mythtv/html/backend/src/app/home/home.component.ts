import { Component, OnInit } from '@angular/core';
import { Observable } from 'rxjs';
import { tap } from 'rxjs/operators';

import { MythService } from '../services/myth.service';
import { MythHostName, MythTimeZone, MythConnectionInfo } from '../services/interfaces/myth.interface';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.css']
})
export class HomeComponent implements OnInit {
  m_hostname$!: Observable<MythHostName>;
  m_timezone$!: Observable<MythTimeZone>;
  m_connectionInfo$!: Observable<MythConnectionInfo>;

  constructor(private mythService: MythService) { }

  ngOnInit(): void {
    this.m_hostname$ = this.mythService.GetHostName().pipe(
      tap(data => console.log(data)),
    )
    this.m_timezone$ = this.mythService.GetTimeZone().pipe(
      tap(data => console.log(data)),
    )
    this.m_connectionInfo$ = this.mythService.GetConnectionInfo().pipe(
      tap(data => console.log(data)),
    )
  }
}
