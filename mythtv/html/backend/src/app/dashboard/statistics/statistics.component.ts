import { Component, OnInit } from '@angular/core';
import { TranslatePipe } from '@ngx-translate/core';
import { StatsResponse } from '../../services/interfaces/status.interface';
import { StatusService } from '../../services/status.service';
import { UtilityService } from '../../services/utility.service';
import { TableModule } from 'primeng/table';

@Component({
  selector: 'app-statistics',
  imports: [TranslatePipe, TableModule],
  templateUrl: './statistics.component.html',
  styleUrl: './statistics.component.css',
})
export class StatisticsComponent implements OnInit {

  statistics?: StatsResponse;

  constructor(private statusService: StatusService, public utility: UtilityService) { }

  ngOnInit(): void {
    this.statusService.GetRecStats().subscribe((stats) => {
      this.statistics = stats;
    });
  }

  formatperiod(secs?: number): string {
    if (!secs)
      return '';
    const years = Math.floor(secs / 31536000);
    secs = secs % 31536000;
    const months = Math.floor(secs / 2592000);
    secs = secs % 2592000;
    const days = Math.floor(secs / 86400);
    secs = secs % 86400;
    const hours = Math.floor(secs / 3600);
    secs = secs % 3600;
    const minutes = Math.floor(secs / 60);
    secs = secs % 60;
    let resp = '';
    if (years > 0)
      resp += years + ' yr ';
    if (months > 0)
      resp += months + ' mth ';
    if (days > 1)
      resp += days + ' days ';
    else if (days === 1)
      resp += days + ' day ';
    if (hours > 0)
      resp += hours + ' hr ';
    if (minutes > 0)
      resp += minutes + ' min ';
    // if (secs > 0)
    // resp += secs + ' sec ';
    return resp;
  }

}
