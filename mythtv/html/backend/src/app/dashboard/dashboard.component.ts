import { Component, OnDestroy, OnInit } from '@angular/core';
import { TranslateService} from '@ngx-translate/core';
import { MenuItem } from 'primeng/api';
import { RouterOutlet, RouterLink, Router, NavigationEnd } from '@angular/router';
import { TooltipModule } from 'primeng/tooltip';
import { RippleModule } from 'primeng/ripple';
import { ButtonModule } from 'primeng/button';
import { TabsModule } from 'primeng/tabs';
import { NgClass } from '@angular/common';
import { Subscription } from 'rxjs';

@Component({
    selector: 'app-dashboard',
    templateUrl: './dashboard.component.html',
    styleUrls: ['./dashboard.component.css'],
    imports: [ButtonModule, RippleModule, TooltipModule, TabsModule, RouterOutlet, RouterLink, NgClass]
})
export class DashboardComponent implements OnInit, OnDestroy {

    translateDone = false;

    fullMenu: MenuItem[] = [
        { label: 'dashboard.backendStatus', routerLink: 'status' },
        { label: 'dashboard.statistics.menu', routerLink: 'statistics' },
        { label: 'dashboard.channeleditor', routerLink: 'channel-editor' },
        { label: 'dashboard.programguide', routerLink: 'program-guide' },
        { label: 'dashboard.recordings.heading', routerLink: 'recordings' },
        { label: 'dashboard.prevrecs.heading', routerLink: 'prev-recorded' },
        { label: 'dashboard.upcoming.heading', routerLink: 'upcoming' },
        { label: 'dashboard.recrules.heading', routerLink: 'recrules' },
        { label: 'dashboard.videos.heading', routerLink: 'videos' },
        { label: 'dashboard.settings.heading', routerLink: 'settings' },
    ]

    tabClass: string[] = [];
    sub?: Subscription;

    constructor(private translate: TranslateService, private router: Router) {
        this.fullMenu.forEach(entry => {
            if (entry.label)
                this.translate.stream(entry.label).subscribe(data => {
                    entry.label = data;
                    this.translateDone = true;
                });
        });
        this.sub = this.router.events.subscribe((event) => {
            if (event instanceof NavigationEnd) {
                this.UpdateMenu();
            }
        });
    }

    ngOnDestroy(): void {
        this.sub?.unsubscribe();
    }


    ngOnInit(): void {
    }

    UpdateMenu() {
        let url = window.location.href;
        let parts = url.split('/');
        let route = parts[parts.length - 1].split('?');
        let tab = this.fullMenu.findIndex((el) => el.routerLink == route[0]);
        this.tabClass = [];
        this.tabClass[tab] = 'tabselected';
    }

}