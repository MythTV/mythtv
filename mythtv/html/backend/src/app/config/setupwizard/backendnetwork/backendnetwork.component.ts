import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';
// import { MythCountryList, MythLanguageList, Country, Language } from '../../../services/interfaces/config.interface';
import { ConfigService } from '../../../services/config.service';

@Component({
    selector: 'app-backendnetwork',
    templateUrl: './backendnetwork.component.html',
    styleUrls: ['./backendnetwork.component.css']
})
export class BackendnetworkComponent implements OnInit {

    constructor(private router: Router,
        private configService: ConfigService) { }

    ngOnInit(): void {
    }

    previousPage() {
        this.router.navigate(['settings/dbsetup']);
        return;
    }


    nextPage() {
        this.router.navigate(['settings/sgsetup']);
        return;
    }
}
