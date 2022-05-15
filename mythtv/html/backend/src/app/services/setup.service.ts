import { Injectable } from '@angular/core';
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

    constructor(private mythService: MythService) {
        this.mythService.GetHostName().subscribe(data => {
            this.m_hostName = data.String;
        });
    }

    Init(): void {
        this.loadSettings();
    }

    loadSettings() {
        // host address
        console.log("Setup service loading settings")
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
    }

    getSetupData(): Setup {
        return this.m_setupData;
    }

    m_miscellaneousData: Miscellaneous = {
        successCount:           0,
        errorCount:             0,
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
        this.m_miscellaneousData.errorCount = 0;
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MasterBackendOverride" })
            .subscribe({next: data => this.m_miscellaneousData.MasterBackendOverride = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DeletesFollowLinks" })
            .subscribe({next: data => this.m_miscellaneousData.DeletesFollowLinks = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "TruncateDeletesSlowly" })
            .subscribe({next: data => this.m_miscellaneousData.TruncateDeletesSlowly = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "HDRingbufferSize" })
            .subscribe({next: data => this.m_miscellaneousData.HDRingbufferSize = Number(data.String),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "StorageScheduler" })
            .subscribe({next: data => this.m_miscellaneousData.StorageScheduler = data.String,
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UPNPWmpSource" })
            .subscribe({next: data => this.m_miscellaneousData.UPNPWmpSource = data.String,
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "MiscStatusScript" })
            .subscribe({next: data => this.m_miscellaneousData.MiscStatusScript = data.String,
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DisableAutomaticBackup" })
            .subscribe({next: data => this.m_miscellaneousData.DisableAutomaticBackup = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "DisableFirewireReset" })
            .subscribe({next: data => this.m_miscellaneousData.DisableFirewireReset = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});

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

    miscObserver = {
        next: (x: any) => {
            console.log('Observer got a next value: ' + x.bool);
            if (x.bool)
                this.m_miscellaneousData.successCount ++;
            else
                this.m_miscellaneousData.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_miscellaneousData.errorCount ++
        },
        complete: () => console.log('Observer got a complete notification'),
    };

    saveMiscellaneousSettings () {
        this.m_miscellaneousData.successCount = 0;
        this.m_miscellaneousData.errorCount = 0;
        this.mythService.PutSetting({ HostName: '_GLOBAL_', Key: "MasterBackendOverride",
            Value: this.m_miscellaneousData.MasterBackendOverride ? "1" : "0"})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: '_GLOBAL_', Key: "DeletesFollowLinks",
            Value: this.m_miscellaneousData.DeletesFollowLinks ? "1" : "0"})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: this.m_hostName, Key: "TruncateDeletesSlowly",
            Value: this.m_miscellaneousData.TruncateDeletesSlowly ? "1" : "0"})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: '_GLOBAL_', Key: "HDRingbufferSize",
            Value: String(this.m_miscellaneousData.HDRingbufferSize)})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: '_GLOBAL_', Key: "StorageScheduler",
            Value: this.m_miscellaneousData.StorageScheduler})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: '_GLOBAL_', Key: "UPNPWmpSource",
            Value: this.m_miscellaneousData.UPNPWmpSource})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: this.m_hostName, Key: "MiscStatusScript",
            Value: this.m_miscellaneousData.MiscStatusScript})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: '_GLOBAL_', Key: "DisableAutomaticBackup",
            Value: this.m_miscellaneousData.DisableAutomaticBackup ? "1" : "0"})
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({ HostName: this.m_hostName, Key: "DisableFirewireReset",
            Value: this.m_miscellaneousData.DisableFirewireReset ? "1" : "0"})
            .subscribe(this.miscObserver);
    }
}
