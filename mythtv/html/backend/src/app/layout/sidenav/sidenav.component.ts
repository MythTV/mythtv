import { Component, isDevMode, OnInit, ViewChild } from '@angular/core';
import { Subscription } from 'rxjs';
import { DataService } from 'src/app/services/data.service';
import { MenuItem } from 'primeng/api';
import { TranslateService } from '@ngx-translate/core';
import { Menu } from 'primeng/menu';

@Component({
    selector: 'app-sidenav',
    templateUrl: './sidenav.component.html',
    styleUrls: ['./sidenav.component.css']
})

export class SidenavComponent implements OnInit {

    m_showSidebar: boolean = true;
    m_showSidebarSub!: Subscription;

    m_devMode: boolean = isDevMode();
    m_haveDatabase: boolean = true;

    m_setupMenu: MenuItem[]  = [ {
        // See below for labels
        label: "",
        items: [
            {routerLink: 'settings/general'},
            { routerLink: 'settings/capture-cards'},
            { routerLink: ''},
            { routerLink: 'settings/video-sources'},
            { routerLink: 'settings/input-connections'},
            { routerLink: ''},
            { routerLink: 'settings/storage-groups'},
            { routerLink: ''},
        ]
    }];

    @ViewChild("menu")
    menu!: Menu;

    constructor(private dataService: DataService, private translate: TranslateService) { }

    ngOnInit(): void {
        this.m_showSidebarSub = this.dataService.showSideBar.subscribe(data => this.m_showSidebar = data)
        this.translate.get('sidenav.settingsmenu.title').subscribe(data => this.m_setupMenu[0].label = data);
        this.translate.get('sidenav.settingsmenu.1_general').subscribe(data => this.m_setupMenu[0].items![0].label = data);
        this.translate.get('sidenav.settingsmenu.2_capture_cards').subscribe(data => this.m_setupMenu[0].items![1].label = data);
        this.translate.get('sidenav.settingsmenu.3_recording_profiles').subscribe(data => this.m_setupMenu[0].items![2].label = data);
        this.translate.get('sidenav.settingsmenu.4_video_sources').subscribe(data => this.m_setupMenu[0].items![3].label = data);
        this.translate.get('sidenav.settingsmenu.5_input_connections').subscribe(data => this.m_setupMenu[0].items![4].label = data);
        this.translate.get('sidenav.settingsmenu.6_channel_editor').subscribe(data => this.m_setupMenu[0].items![5].label = data);
        this.translate.get('sidenav.settingsmenu.7_storage_directories').subscribe(data => this.m_setupMenu[0].items![6].label = data);
        this.translate.get('sidenav.settingsmenu.8_system_events').subscribe(data => this.m_setupMenu[0].items![7].label = data);
    }
}
