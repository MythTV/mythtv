import { Component, OnInit } from '@angular/core';
import { ConfigService } from 'src/app/services/config.service';
import { Setup } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-host-address',
    templateUrl: './host-address.component.html',
    styleUrls: ['./host-address.component.css']
})
export class HostAddressComponent implements OnInit {
    m_setupData!: Setup;
    m_showHelp: boolean = false;

    m_IPsAll!: string[];
    m_IPsV4!: string[];
    m_IPsV6!: string[];

    constructor(private setupService: SetupService,
                private configService: ConfigService) {

        configService.GetIPAddresses("All").subscribe(result => this.m_IPsAll = result.IPAddresses);
        configService.GetIPAddresses("IPv4").subscribe(result => this.m_IPsV4 = result.IPAddresses);
        configService.GetIPAddresses("IPv6").subscribe(result => this.m_IPsV6 = result.IPAddresses);
    }

    ngOnInit(): void {
        this.m_setupData = this.setupService.getSetupData();
    }

    showHelp() {
        this.m_showHelp = true;
    }

    saveForm() {
        console.log("save form clicked");
    }
}
