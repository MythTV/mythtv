import { Component, isDevMode, OnInit } from '@angular/core';
import { Subscription } from 'rxjs';
import { DataService } from 'src/app/services/data.service';

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

    constructor(private dataService: DataService) { }

    ngOnInit(): void {
        this.m_showSidebarSub = this.dataService.showSideBar.subscribe(data => this.m_showSidebar = data)
    }
}
