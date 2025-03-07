import { Component, isDevMode, OnInit, ViewChild } from '@angular/core';
import { DataService } from 'src/app/services/data.service';
import { RouterLink, RouterOutlet } from '@angular/router';
import { Tooltip } from 'primeng/tooltip';
import { Ripple } from 'primeng/ripple';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-sidenav',
    templateUrl: './sidenav.component.html',
    styleUrls: ['./sidenav.component.css'],
    standalone: true,
    imports: [RouterLink, Tooltip, Ripple, RouterOutlet, TranslatePipe]
})

export class SidenavComponent implements OnInit {

    constructor(public dataService: DataService) { }

    ngOnInit(): void {
    }
}
