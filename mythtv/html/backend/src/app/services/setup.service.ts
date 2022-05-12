import { Injectable } from '@angular/core';
import { subscribeOn, tap } from 'rxjs/operators';
import { Setup, Miscellaneous } from './interfaces/setup.interface';
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
            },
        }
    }


    // TODO: Move the initialization to a called function so you don't have to initialize these
    // if you are not using the Host Address Backend Setup or Locale tab
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

    m_miscellaneousData: Miscellaneous = {
        MasterBackendOverride:  false,
        DeletesFollowLinks:     false,
        TruncateDeletesSlowly:  false,
        HDRingbufferSize:       9400,
        StorageScheduler:       "BalancedFreeSpace",
        UPNPWmpSource:          "0",
        MiscStatusScript:       "",
        DisableAutomaticBackup: false,
        DisableFirewireReset:   false,
    }

    getMiscellaneousData () : Miscellaneous  {
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MasterBackendOverride" })
            .subscribe(data => this.m_miscellaneousData.MasterBackendOverride = (data.String == "1"));
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DeletesFollowLinks" })
            .subscribe(data => this.m_miscellaneousData.DeletesFollowLinks = (data.String == "1"));
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "TruncateDeletesSlowly" })
            .subscribe(data => this.m_miscellaneousData.TruncateDeletesSlowly = (data.String == "1"));
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "HDRingbufferSize" })
            .subscribe(data => this.m_miscellaneousData.HDRingbufferSize = Number(data.String));
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "StorageScheduler" })
            .subscribe(data => this.m_miscellaneousData.StorageScheduler = data.String);
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UPNPWmpSource" })
            .subscribe(data => this.m_miscellaneousData.UPNPWmpSource = data.String);
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "MiscStatusScript" })
            .subscribe(data => this.m_miscellaneousData.MiscStatusScript = data.String);
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DisableAutomaticBackup" })
            .subscribe(data => this.m_miscellaneousData.DisableAutomaticBackup = (data.String == "1"));
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "DisableFirewireReset" })
            .subscribe(data => this.m_miscellaneousData.DisableFirewireReset = (data.String == "1"));
            return this.m_miscellaneousData;
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
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "TVFormat", Value: this.m_setupData.General.Locale.TVFormat }).subscribe(result => { console.log("TVFormat: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "VbiFormat", Value: this.m_setupData.General.Locale.VbiFormat }).subscribe(result => { console.log("VBIFormat: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "FreqTable", Value: this.m_setupData.General.Locale.FreqTable }).subscribe(result => { console.log("FreqTable: ", result.bool); });
    }
}
