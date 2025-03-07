import { Component, OnInit } from '@angular/core';
import { TranslateService, TranslatePipe } from '@ngx-translate/core';
import { MenuItem } from 'primeng/api';
import { SetupService } from '../services/setup.service';
import { NgIf } from '@angular/common';
import { ButtonDirective } from 'primeng/button';
import { Ripple } from 'primeng/ripple';
import { Tooltip } from 'primeng/tooltip';
import { TabMenu } from 'primeng/tabmenu';
import { RouterOutlet } from '@angular/router';

@Component({
    selector: 'app-dashboard',
    templateUrl: './dashboard.component.html',
    styleUrls: ['./dashboard.component.css'],
    standalone: true,
    imports: [NgIf, ButtonDirective, Ripple, Tooltip, TabMenu, RouterOutlet, TranslatePipe]
})
export class DashboardComponent implements OnInit {

  translateDone = false;

  fullMenu: MenuItem[] = [
    { label: 'dashboard.backendStatus', routerLink: 'status' },
    { label: 'dashboard.channeleditor', routerLink: 'channel-editor' },
    { label: 'dashboard.programguide', routerLink: 'program-guide' },
    { label: 'dashboard.recordings.heading', routerLink: 'recordings' },
    { label: 'dashboard.prevrecs.heading', routerLink: 'prev-recorded' },
    { label: 'dashboard.upcoming.heading', routerLink: 'upcoming' },
    { label: 'dashboard.recrules.heading', routerLink: 'recrules' },
    { label: 'dashboard.videos.heading', routerLink: 'videos' },
    { label: 'dashboard.settings.heading', routerLink: 'settings' },
  ]

  activeItem = this.fullMenu[0];

  constructor(private translate: TranslateService, private setupService: SetupService) {
    setupService.pageType = 'D';
    this.fullMenu.forEach(entry => {
      if (entry.label)
        this.translate.get(entry.label).subscribe(data => {
          entry.label = data;
          this.translateDone = true;
        });
    });
  }

  ngOnInit(): void {
  }

}
