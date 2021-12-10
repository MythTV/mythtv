import { Component, OnInit } from '@angular/core';
import { MythService } from '../services/myth.service';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.css']
})
export class HomeComponent implements OnInit {
  hostname = '';
  tz_id = 'unknown';
  tz_datetime = 'unknown';
  tz_offset = 0;

  constructor(private mythService: MythService) { }

  ngOnInit(): void {
    this.mythService.GetHostName().subscribe(data => {
      console.log(data);
      this.hostname = data.String;
    });
    this.mythService.GetTimeZone().subscribe(data => {
      console.log(data);
      this.tz_id = data.TimeZoneInfo.TimeZoneID;
      this.tz_datetime = data.TimeZoneInfo.CurrentDateTime;
      this.tz_offset = data.TimeZoneInfo.UTCOffset;
    })
  }

}
