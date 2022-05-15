import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';
import { Setup } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';
import { ConfigService } from '../../../services/config.service';

@Component({
    selector: 'app-backendnetwork',
    templateUrl: './backendnetwork.component.html',
    styleUrls: ['./backendnetwork.component.css']
})
export class BackendnetworkComponent implements OnInit {

    m_setupData!: Setup;
    m_showHelp: boolean = false;

    m_IPsAll!: string[];
    m_IPsV4!: string[];
    m_IPsV6!: string[];

    constructor(private router: Router,
                private configService: ConfigService,
                private setupService: SetupService) {
    }

    ngOnInit(): void {
        this.m_setupData = this.setupService.getSetupData();
        this.configService.GetIPAddresses("All").subscribe(result => this.m_IPsAll = result.IPAddresses);
        this.configService.GetIPAddresses("IPv4").subscribe(result => this.m_IPsV4 = result.IPAddresses);
        this.configService.GetIPAddresses("IPv6").subscribe(result => this.m_IPsV6 = result.IPAddresses);
    }

    previousPage() {
        this.router.navigate(['settings/dbsetup']);
        return;
    }

    nextPage() {
        this.router.navigate(['settings/sgsetup']);
        return;
    }

    showHelp() {
        this.m_showHelp = true;
    }

    saveForm() {
        console.log("save form clicked");
    }
}
