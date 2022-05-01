import { Component, OnInit } from '@angular/core';
import { Observable, throwError } from 'rxjs';
import { catchError, tap } from 'rxjs/operators';

import { MythService } from '../services/myth.service';
import { ConfigService } from '../services/config.service';
import { MythHostName, MythTimeZone, MythConnectionInfo, GetSettingResponse } from '../services/interfaces/myth.interface';
import { MythDatabaseStatus } from '../services/interfaces/config.interface';
import { HttpErrorResponse } from '@angular/common/http';

@Component({
  selector: 'app-dashboard',
  templateUrl: './dashboard.component.html',
  styleUrls: ['./dashboard.component.css']
})
export class DashboardComponent implements OnInit {
  m_hostname$!: Observable<MythHostName>;
  m_timezone$!: Observable<MythTimeZone>;
  m_connectionInfo$!: Observable<MythConnectionInfo>;
  m_setting$!: Observable<GetSettingResponse>;
  m_databaseStatus$!: Observable<MythDatabaseStatus>

  public errorRes!: HttpErrorResponse;

  constructor(private mythService: MythService, private configService: ConfigService) { }

  ngOnInit(): void {
    this.m_hostname$ = this.mythService.GetHostName().pipe(
      tap(data => console.log(data)),
    )

    this.m_timezone$ = this.mythService.GetTimeZone().pipe(
      tap(data => console.log(data)),
    )

    // TODO: This call fails without a PIN set
    this.m_connectionInfo$ = this.mythService.GetConnectionInfo("").pipe(
      catchError((err: HttpErrorResponse) => {
        console.error("error getting connection info", err);
        this.errorRes = err;
        return throwError(err);
      })
    )

    this.m_setting$ = this.mythService.GetSetting({HostName: "localhost", Key: "TestSetting"}).pipe(
      tap(data => console.log(data)),
    )

    this.m_databaseStatus$ = this.configService.GetDatabaseStatus().pipe(
      tap(data => console.log("Database Status: " + data)),
    )
  }
}
