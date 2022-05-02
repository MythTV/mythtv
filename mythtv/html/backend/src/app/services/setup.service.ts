import { Injectable } from '@angular/core';
import { subscribeOn, tap } from 'rxjs/operators';
import { Setup } from './interfaces/setup.interface';
import { MythService } from './myth.service';

@Injectable({
    providedIn: 'root'
})
export class SetupService {

    m_hostName: string = ""; // hostname of the backend server

    m_setupData: Setup = {
        General: {
            HostAddress: {
                BackendServerPort: 4543,
                BackendStatusPort: 4544,
                SecurityPin: '0000',
                AllowConnFromAll: false,
                ListenOnAllIps: true,
                BackendServerIP: '127.0.0.1',
                BackendServerIP6: '::1',
                AllowLinkLocal: true,
                BackendServerAddr: '',
                IsMasterBackend: true,
                MasterServerName: '',
            },
            Locale: {
                TVFormat: 'PAL',
                VbiFormat: 'None',
                FreqTable: 'us-bcast'
            }
        }
    }

    constructor(private mythService: MythService) {
        this.mythService.GetHostName().subscribe(data => {
            this.m_hostName = data.String;
            // host address
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerPort" }).subscribe(data => this.m_setupData.General.HostAddress.BackendServerPort = Number(data.String));
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendStatusPort" }).subscribe(data => this.m_setupData.General.HostAddress.BackendStatusPort = Number(data.String));
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "SecurityPin" }).subscribe(data => this.m_setupData.General.HostAddress.SecurityPin = data.String);
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "AllowConnFromAll" }).subscribe(data => this.m_setupData.General.HostAddress.AllowConnFromAll = (data.String == "1"));
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "ListenOnAllIps" }).subscribe(data => this.m_setupData.General.HostAddress.ListenOnAllIps = (data.String == "1"));
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerIP" }).subscribe(data => this.m_setupData.General.HostAddress.BackendServerIP = data.String);
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerIP6" }).subscribe(data => this.m_setupData.General.HostAddress.BackendServerIP6 = data.String);
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "UseLinkLocal" }).subscribe(data => this.m_setupData.General.HostAddress.AllowLinkLocal = (data.String == "1"));
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerAddr" }).subscribe(data => this.m_setupData.General.HostAddress.BackendServerAddr = data.String);
            this.mythService.GetSetting({ HostName: this.m_hostName, Key: "IsMasterBackend" }).subscribe(data => this.m_setupData.General.HostAddress.IsMasterBackend = (data.String == "1"));
            this.mythService.GetSetting({ HostName: '_GLOBAL_'     , Key: "MasterServerName" }).subscribe(data => this.m_setupData.General.HostAddress.MasterServerName = data.String);
            // locale
            this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "TVFormat" }).subscribe(data => this.m_setupData.General.Locale.TVFormat = data.String);
            this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "VbiFormat" }).subscribe(data => this.m_setupData.General.Locale.VbiFormat = data.String);
            this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "FreqTable" }).subscribe(data => this.m_setupData.General.Locale.FreqTable = data.String);
        });



     }

    getSetupData(): Setup {
        return this.m_setupData;
    }

    saveHostAddressSettings() {
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "BackendServerPort", Value: String(this.m_setupData.General.HostAddress.BackendServerPort) }).subscribe(result => { console.log("BackendServerPort: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "BackendStatusPort", Value: String(this.m_setupData.General.HostAddress.BackendStatusPort) }).subscribe(result => { console.log("BackendStatusPort: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "SecurityPin",       Value: this.m_setupData.General.HostAddress.SecurityPin }).subscribe(result => { console.log("SecurityPin: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "AllowConnFromAll",  Value: this.m_setupData.General.HostAddress.AllowConnFromAll ? "1" : "0" }).subscribe(result => { console.log("AllowConnFromAll: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "ListenOnAllIps",    Value: String(this.m_setupData.General.HostAddress.ListenOnAllIps) }).subscribe(result => { console.log("ListenOnAllIps: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "BackendServerIP",   Value: this.m_setupData.General.HostAddress.BackendServerIP }).subscribe(result => { console.log("LocalServerIP: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "BackendServerIP6",  Value: this.m_setupData.General.HostAddress.BackendServerIP6 }).subscribe(result => { console.log("LocalServerIP6: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "AllowLinkLocal",    Value: this.m_setupData.General.HostAddress.AllowLinkLocal ? "1" : "0" }).subscribe(result => { console.log("UseLinkLocal: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "BackendServerAddr", Value: this.m_setupData.General.HostAddress.BackendServerAddr }).subscribe(result => { console.log("BackendServerAddr: ", result.bool); });
        this.mythService.PutSetting({ HostName: "this.m_hostName", Key: "IsMasterBackend",   Value: this.m_setupData.General.HostAddress.IsMasterBackend ? "1" : "0" }).subscribe(result => { console.log("IsMasterBackend: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "MasterServerName", Value: this.m_setupData.General.HostAddress.MasterServerName }).subscribe(result => { console.log("MasterServerName: ", result.bool); });

    }

    saveLocaleSettings() {
        console.log("TVFormat: ", this.m_setupData.General.Locale.TVFormat,"VbiFormat: ", this.m_setupData.General.Locale.VbiFormat, "FreqTable: ", this.m_setupData.General.Locale.FreqTable);
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "TVFormat", Value: this.m_setupData.General.Locale.TVFormat }).subscribe(result => { console.log("TVFormat: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "VbiFormat", Value: this.m_setupData.General.Locale.VbiFormat }).subscribe(result => { console.log("VBIFormat: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "FreqTable", Value: this.m_setupData.General.Locale.FreqTable }).subscribe(result => { console.log("FreqTable: ", result.bool); });
        //this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "TVFormat", Value: this.m_setupData.General.Locale.TVFormat });
        //this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "VbiFormat", Value: this.m_setupData.General.Locale.VbiFormat });
        //this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "FreqTable", Value: this.m_setupData.General.Locale.FreqTable });
    }
}
