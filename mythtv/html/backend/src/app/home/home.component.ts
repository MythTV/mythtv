import { Component, OnInit } from '@angular/core';
import { Observable, throwError } from 'rxjs';
import { catchError, tap } from 'rxjs/operators';

import { MythService } from '../services/myth.service';
import { MythHostName, MythTimeZone, MythConnectionInfo, GetSettingResponse } from '../services/interfaces/myth.interface';
import { HttpErrorResponse } from '@angular/common/http';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.css']
})
export class HomeComponent implements OnInit {
  m_hostname$!: Observable<MythHostName>;
  m_timezone$!: Observable<MythTimeZone>;
  m_connectionInfo$!: Observable<MythConnectionInfo>;
  m_setting$!: Observable<GetSettingResponse>;

  public errorRes!: HttpErrorResponse;

  constructor(private mythService: MythService) { }

  ngOnInit(): void {
    this.m_hostname$ = this.mythService.GetHostName().pipe(
      tap(data => console.log(data)),
    )

    this.m_timezone$ = this.mythService.GetTimeZone().pipe(
      tap(data => console.log(data)),
    )

    this.m_connectionInfo$ = this.mythService.GetConnectionInfo().pipe(
      catchError((err: HttpErrorResponse) => {
        console.error("error getting connection info", err);
        this.errorRes = err;
        return throwError(err);
      })
    )

    this.m_setting$ = this.mythService.GetSetting({HostName: "localhost", Key: "TestSetting"}).pipe(
      tap(data => console.log(data)),
    )
  }
}
