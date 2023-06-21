import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { ConfigService } from 'src/app/services/config.service';

import { HostAddress } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
    selector: 'app-host-address',
    templateUrl: './host-address.component.html',
    styleUrls: ['./host-address.component.css']
})
export class HostAddressComponent implements OnInit, AfterViewInit {
    m_HostAddressData!: HostAddress;

    m_IPsAll!: string[];
    m_IPsV4!: string[];
    m_IPsV6!: string[];
    m_IsMasterBackend: boolean = false;

    @ViewChild("hostaddress")
    currentForm!: NgForm;

    constructor(public setupService: SetupService,
        private configService: ConfigService) {

        this.m_HostAddressData = this.setupService.getHostAddressData();
        // Set this for 500 msgit log later to avoid messages that value changed after checked
        // (pristine) and also avoid form being set as dirty.
        setTimeout(() =>
            configService.GetIPAddresses("All").subscribe(result => this.m_IPsAll = result.IPAddresses)
            , 500)
        configService.GetIPAddresses("IPv4").subscribe(result => this.m_IPsV4 = result.IPAddresses);
        configService.GetIPAddresses("IPv6").subscribe(result => this.m_IPsV6 = result.IPAddresses);
    }

    ngOnInit(): void {
    }

    ngAfterViewInit() {
        this.setupService.setCurrentForm(this.currentForm);
    }

    m_savedMaster!: string;
    m_showChangeHint = false;
    // Called when they check or uncheck "This server is the Master Backend"
    // so we can set the master backend.
    setMaster() {
        if (typeof this.m_savedMaster == 'undefined')
            this.m_savedMaster = this.m_HostAddressData.MasterServerName;
        if (this.m_HostAddressData.IsMasterBackend) {
            this.m_HostAddressData.MasterServerName = this.m_HostAddressData.thisHostName;
            this.m_showChangeHint = false;
        }
        else {
            if (this.m_HostAddressData.MasterServerName == this.m_savedMaster)
                this.m_showChangeHint = true;
            else
                this.m_HostAddressData.MasterServerName = this.m_savedMaster;
        }
    }

    saveForm() {
        console.log("save form clicked");
        this.setupService.saveHostAddressData(this.currentForm);
    }
}
