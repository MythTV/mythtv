import { Component, isDevMode, OnInit, ViewChild } from '@angular/core';
import { Subscription } from 'rxjs';

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


    constructor() { }

    ngOnInit(): void {
    }
}
