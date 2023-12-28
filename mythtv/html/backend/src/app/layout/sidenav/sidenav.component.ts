import { Component, isDevMode, OnInit, ViewChild } from '@angular/core';
import { DataService } from 'src/app/services/data.service';

@Component({
    selector: 'app-sidenav',
    templateUrl: './sidenav.component.html',
    styleUrls: ['./sidenav.component.css']
})

export class SidenavComponent implements OnInit {

    constructor(public dataService: DataService) { }

    ngOnInit(): void {
    }
}
