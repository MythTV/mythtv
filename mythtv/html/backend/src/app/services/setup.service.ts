import { Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { GetSettingResponse } from './interfaces/myth.interface';
import { Setup, Miscellaneous, EITScanner, ShutWake, BackendWake, BackendControl,
    JobQBackend, JobQCommands, JobQGlobal }
    from './interfaces/setup.interface';
import { MythService } from './myth.service';

@Injectable({
    providedIn: 'root'
})
export class SetupService {

    m_hostName: string = ""; // hostname of the backend server
    m_initialized: boolean = false;

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
        this.m_initialized = true;
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
        if (!this.m_initialized) {
            this.Init();
        }
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
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "TVFormat", Value: this.m_setupData.General.Locale.TVFormat }).subscribe(result => { console.log("TVFormat: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "VbiFormat", Value: this.m_setupData.General.Locale.VbiFormat }).subscribe(result => { console.log("VBIFormat: ", result.bool); });
        this.mythService.PutSetting({ HostName: "_GLOBAL_", Key: "FreqTable", Value: this.m_setupData.General.Locale.FreqTable }).subscribe(result => { console.log("FreqTable: ", result.bool); });
    }

    m_miscellaneousData!: Miscellaneous;

    getMiscellaneousData () : Miscellaneous  {
        this.m_miscellaneousData = {
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
        };

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MasterBackendOverride", Default: "0" })
            .subscribe({next: data => this.m_miscellaneousData.MasterBackendOverride = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DeletesFollowLinks", Default: "0" })
            .subscribe({next: data => this.m_miscellaneousData.DeletesFollowLinks = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "TruncateDeletesSlowly", Default: "0" })
            .subscribe({next: data => this.m_miscellaneousData.TruncateDeletesSlowly = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "HDRingbufferSize", Default: "9400" })
            .subscribe({next: data => this.m_miscellaneousData.HDRingbufferSize = Number(data.String),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "StorageScheduler", Default: "BalancedFreeSpace" })
            .subscribe({next: data => this.m_miscellaneousData.StorageScheduler = data.String,
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UPNPWmpSource", Default: "0" })
            .subscribe({next: data => this.m_miscellaneousData.UPNPWmpSource = data.String,
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "MiscStatusScript", Default: "" })
            .subscribe({next: data => this.m_miscellaneousData.MiscStatusScript = data.String,
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DisableAutomaticBackup", Default: "0" })
            .subscribe({next: data => this.m_miscellaneousData.DisableAutomaticBackup = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "DisableFirewireReset", Default: "0" })
            .subscribe({next: data => this.m_miscellaneousData.DisableFirewireReset = (data.String == "1"),
            error: () => this.m_miscellaneousData.errorCount++});

        return this.m_miscellaneousData;
    }

    miscObserver = {
        next: (x: any) => {
            if (x.bool)
                this.m_miscellaneousData.successCount ++;
            else
                this.m_miscellaneousData.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_miscellaneousData.errorCount ++
        }
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

    m_EITScanner!: EITScanner;

    getEITScanner(): EITScanner {
        this.m_EITScanner = {
            successCount: 0,
            errorCount: 0,
            EITTransportTimeout: 5,
            EITCrawIdleStart: 60
        }

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "EITTransportTimeout", Default : "5" })
            .subscribe({next: data => this.m_EITScanner.EITTransportTimeout = Number(data.String),
                error: () => this.m_EITScanner.errorCount++});
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "EITCrawIdleStart", Default: "60" })
            .subscribe({next: data => this.m_EITScanner.EITCrawIdleStart = Number(data.String),
                error: () => this.m_EITScanner.errorCount++});

        return this.m_EITScanner;
    }

    eitObserver = {
        next: (x: any) => {
            if (x.bool)
                this.m_EITScanner.successCount++;
            else
                this.m_EITScanner.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_EITScanner.errorCount++
        },
    };

    saveEITScanner() {
        this.m_EITScanner.successCount = 0;
        this.m_EITScanner.errorCount = 0;
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "EITTransportTimeout",
            Value: String(this.m_EITScanner.EITTransportTimeout)}).subscribe(this.eitObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "EITCrawIdleStart",
            Value: String(this.m_EITScanner.EITCrawIdleStart)}).subscribe(this.eitObserver);
    }

    m_ShutWake!: ShutWake;

    getShutWake(): ShutWake {
        this.m_ShutWake = {
            successCount: 0,
            errorCount: 0,
            startupCommand:             "",
            blockSDWUwithoutClient:     true,
            idleTimeoutSecs:            0,
            idleWaitForRecordingTime:   15,
            StartupSecsBeforeRecording: 120,
            WakeupTimeFormat:           "hh:mm yyyy-MM-dd",
            SetWakeuptimeCommand:       "",
            ServerHaltCommand:          "sudo /sbin/halt -p",
            preSDWUCheckCommand:        ""
        }

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "startupCommand", Default: "" })
            .subscribe({
                next: data => this.m_ShutWake.startupCommand = data.String,
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "blockSDWUwithoutClient", Default: "1" })
            .subscribe({
                next: data => this.m_ShutWake.blockSDWUwithoutClient = (data.String == '1'),
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "idleTimeoutSecs", Default: "0" })
            .subscribe({
                next: data => this.m_ShutWake.idleTimeoutSecs = Number(data.String),
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "idleWaitForRecordingTime", Default: "" })
            .subscribe({
                next: data => this.m_ShutWake.idleWaitForRecordingTime = Number(data.String),
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "StartupSecsBeforeRecording", Default: "120" })
            .subscribe({
                next: data => this.m_ShutWake.StartupSecsBeforeRecording = Number(data.String),
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WakeupTimeFormat", Default: "hh:mm yyyy-MM-dd" })
            .subscribe({
                next: data => this.m_ShutWake.WakeupTimeFormat = data.String,
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "SetWakeuptimeCommand", Default: "" })
            .subscribe({
                next: data => this.m_ShutWake.SetWakeuptimeCommand = data.String,
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "ServerHaltCommand", Default: "sudo /sbin/halt -p" })
            .subscribe({
                next: data => this.m_ShutWake.ServerHaltCommand = data.String,
                error: () => this.m_ShutWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "preSDWUCheckCommand", Default: "" })
            .subscribe({
                next: data => this.m_ShutWake.preSDWUCheckCommand = data.String,
                error: () => this.m_ShutWake.errorCount++
            });

        return this.m_ShutWake;
    }

    swObserver = {
        next: (x: any) => {
            if (x.bool)
                this.m_ShutWake.successCount++;
            else
                this.m_ShutWake.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_ShutWake.errorCount++
        },
    };

    saveShutWake() {
        this.m_ShutWake.successCount = 0;
        this.m_ShutWake.errorCount = 0;
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "startupCommand",
            Value: this.m_ShutWake.startupCommand}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "blockSDWUwithoutClient",
            Value: this.m_ShutWake.blockSDWUwithoutClient ? "1" : "0"}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "idleTimeoutSecs",
            Value: String(this.m_ShutWake.idleTimeoutSecs)}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "idleWaitForRecordingTime",
            Value: String(this.m_ShutWake.idleWaitForRecordingTime)}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "StartupSecsBeforeRecording",
            Value: String(this.m_ShutWake.StartupSecsBeforeRecording)}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "WakeupTimeFormat",
            Value: this.m_ShutWake.WakeupTimeFormat}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "SetWakeuptimeCommand",
            Value: this.m_ShutWake.SetWakeuptimeCommand}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "ServerHaltCommand",
            Value: this.m_ShutWake.ServerHaltCommand}).subscribe(this.swObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "preSDWUCheckCommand",
            Value: this.m_ShutWake.preSDWUCheckCommand}).subscribe(this.swObserver);
    }

    m_BackendWake!: BackendWake;

    getBackendWake(): BackendWake {
        this.m_BackendWake = {
            successCount: 0,
            errorCount: 0,
            WOLbackendReconnectWaitTime:    0,
            WOLbackendConnectRetry:         5,
            WOLbackendCommand:              "",
            SleepCommand:                   "",
            WakeUpCommand:                  ""
        }

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WOLbackendReconnectWaitTime", Default: "0" })
            .subscribe({
                next: data => this.m_BackendWake.WOLbackendReconnectWaitTime = Number(data.String),
                error: () => this.m_BackendWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WOLbackendConnectRetry", Default: "5" })
            .subscribe({
                next: data => this.m_BackendWake.WOLbackendConnectRetry = Number(data.String),
                error: () => this.m_BackendWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "WOLbackendCommand", Default: "" })
            .subscribe({
                next: data => this.m_BackendWake.WOLbackendCommand = data.String,
                error: () => this.m_BackendWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "SleepCommand", Default: "" })
            .subscribe({
                next: data => this.m_BackendWake.SleepCommand = data.String,
                error: () => this.m_BackendWake.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "WakeUpCommand", Default: "" })
            .subscribe({
                next: data => this.m_BackendWake.WakeUpCommand = data.String,
                error: () => this.m_BackendWake.errorCount++
            });

        return this.m_BackendWake;
    }

    bewObserver = {
        next: (x: any) => {
            if (x.bool)
                this.m_BackendWake.successCount++;
            else
                this.m_BackendWake.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_BackendWake.errorCount++
        },
    };

    saveBackendWake() {
        this.m_BackendWake.successCount = 0;
        this.m_BackendWake.errorCount = 0;
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "WOLbackendReconnectWaitTime",
            Value: String(this.m_BackendWake.WOLbackendReconnectWaitTime)}).subscribe(this.bewObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "WOLbackendConnectRetry",
            Value: String(this.m_BackendWake.WOLbackendConnectRetry)}).subscribe(this.bewObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "WOLbackendCommand",
            Value: this.m_BackendWake.WOLbackendCommand}).subscribe(this.bewObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "SleepCommand",
            Value: this.m_BackendWake.SleepCommand}).subscribe(this.bewObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "WakeUpCommand",
            Value: this.m_BackendWake.WakeUpCommand}).subscribe(this.bewObserver);
    }

    m_BackendControl! : BackendControl;

    getBackendControl(): BackendControl {
        this.m_BackendControl = {
            successCount: 0,
            errorCount: 0,
            BackendStopCommand:     "killall mythbackend",
            BackendStartCommand:    "mythbackend"
        }

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "BackendStopCommand", Default: "killall mythbackend" })
            .subscribe({
                next: data => this.m_BackendControl.BackendStopCommand = data.String,
                error: () => this.m_BackendControl.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "BackendStartCommand", Default: "mythbackend" })
            .subscribe({
                next: data => this.m_BackendControl.BackendStartCommand = data.String,
                error: () => this.m_BackendControl.errorCount++
            });

        return this.m_BackendControl;
    }

    becObserver = {
        next: (x: any) => {
            if (x.bool)
                this.m_BackendControl.successCount++;
            else
                this.m_BackendControl.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_BackendControl.errorCount++
        },
    };

    saveBackendControl() {
        this.m_BackendControl.successCount = 0;
        this.m_BackendControl.errorCount = 0;
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "BackendStopCommand",
            Value: this.m_BackendControl.BackendStopCommand}).subscribe(this.becObserver);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "BackendStartCommand",
            Value: this.m_BackendControl.BackendStartCommand}).subscribe(this.becObserver);
    }

    m_JobQBackend!: JobQBackend;

    parseTime(date: Date, string: String) {
        let parts = string.split(':');
        date.setHours(Number(parts[0]));
        date.setMinutes(Number(parts[1]));
    }

    formatTime(date: Date) : string{
        let hours = date.getHours();
        let minutes = date.getMinutes();
        let string = "";
        if (hours < 10) {
            string += "0"
        }
        string += String(hours);
        string += ":";
        if (minutes < 10) {
            string += "0"
        }
        string += String(minutes);
        return string;
    }

    getJobQBackend(): JobQBackend {
        this.m_JobQBackend = {
            successCount: 0,
            errorCount: 0,
            JobQueueMaxSimultaneousJobs:    1,
            JobQueueCheckFrequency:         60,
            JobQueueWindowStart:            new Date(0),
            JobQueueWindowStartObs:         new Observable<GetSettingResponse>(),
            JobQueueWindowEnd:              new Date(0),
            JobQueueWindowEndObs:           new Observable<GetSettingResponse>(),
            JobQueueCPU:                    "0",
            JobAllowMetadata:               true,
            JobAllowCommFlag:               true,
            JobAllowTranscode:              true,
            JobAllowPreview:                true,
            JobAllowUserJob1:               false,
            JobAllowUserJob2:               false,
            JobAllowUserJob3:               false,
            JobAllowUserJob4:               false,
        }
        this.parseTime(this.m_JobQBackend.JobQueueWindowStart,"00:00");
        this.parseTime(this.m_JobQBackend.JobQueueWindowEnd,"23:59");

        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobQueueMaxSimultaneousJobs", Default: "1" })
            .subscribe({
                next: data => this.m_JobQBackend.JobQueueMaxSimultaneousJobs = Number(data.String),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobQueueCheckFrequency", Default: "60" })
            .subscribe({
                next: data => this.m_JobQBackend.JobQueueCheckFrequency = Number(data.String),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.m_JobQBackend.JobQueueWindowStartObs = this.mythService.GetSetting({
             HostName: this.m_hostName, Key: "JobQueueWindowStart", Default: "00:00" });
        this.m_JobQBackend.JobQueueWindowStartObs
            .subscribe({
                next: data => this.parseTime(this.m_JobQBackend.JobQueueWindowStart, data.String),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.m_JobQBackend.JobQueueWindowEndObs = this.mythService.GetSetting({
            HostName: this.m_hostName, Key: "JobQueueWindowEnd", Default: "23:59" });
        this.m_JobQBackend.JobQueueWindowEndObs
            .subscribe({
                next: data => this.parseTime(this.m_JobQBackend.JobQueueWindowEnd, data.String),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobQueueCPU", Default: "0" })
            .subscribe({
                next: data => this.m_JobQBackend.JobQueueCPU =  data.String,
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowMetadata", Default: "1" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowMetadata =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowCommFlag", Default: "1" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowCommFlag =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowTranscode", Default: "1" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowTranscode =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowPreview", Default: "1" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowPreview =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowUserJob1", Default: "0" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowUserJob1 =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowUserJob2", Default: "0" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowUserJob2 =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowUserJob3", Default: "0" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowUserJob3 =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "JobAllowUserJob4", Default: "0" })
            .subscribe({
                next: data => this.m_JobQBackend.JobAllowUserJob4 =  (data.String == '1'),
                error: () => this.m_JobQBackend.errorCount++
            });
        return this.m_JobQBackend;
    }

    jqbObserver = {
        next: (x: any) => {
            if (x.bool)
                this.m_JobQBackend.successCount++;
            else
                this.m_JobQBackend.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_JobQBackend.errorCount++
        },
    };

    saveJobQBackend() {
        this.m_JobQBackend.successCount = 0;
        this.m_JobQBackend.errorCount = 0;
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobQueueMaxSimultaneousJobs",
            Value: String(this.m_JobQBackend.JobQueueMaxSimultaneousJobs)}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobQueueCheckFrequency",
            Value: String(this.m_JobQBackend.JobQueueCheckFrequency)}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobQueueWindowStart",
            Value: this.formatTime(this.m_JobQBackend.JobQueueWindowStart)}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobQueueWindowEnd",
            Value: this.formatTime(this.m_JobQBackend.JobQueueWindowEnd)}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobQueueCPU",
            Value: this.m_JobQBackend.JobQueueCPU}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowMetadata",
            Value: this.m_JobQBackend.JobAllowMetadata ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowCommFlag",
            Value: this.m_JobQBackend.JobAllowCommFlag ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowTranscode",
            Value: this.m_JobQBackend.JobAllowTranscode ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowPreview",
            Value: this.m_JobQBackend.JobAllowPreview ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowUserJob1",
            Value: this.m_JobQBackend.JobAllowUserJob1 ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowUserJob2",
            Value: this.m_JobQBackend.JobAllowUserJob2 ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowUserJob3",
            Value: this.m_JobQBackend.JobAllowUserJob3 ? "1" : "0"}).subscribe(this.jqbObserver);
        this.mythService.PutSetting({HostName: this.m_hostName, Key: "JobAllowUserJob4",
            Value: this.m_JobQBackend.JobAllowUserJob4 ? "1" : "0"}).subscribe(this.jqbObserver);

        }

    m_JobQCommands!: JobQCommands;

    getJobQCommands(): JobQCommands {
        this.m_JobQCommands = {
            successCount: 0,
            errorCount: 0,
            UserJobDesc1:                   "User Job #1",
            UserJobDesc2:                   "User Job #2",
            UserJobDesc3:                   "User Job #3",
            UserJobDesc4:                   "User Job #4"
        }

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJobDesc1", Default: "User Job #1" })
            .subscribe({
                next: data => this.m_JobQCommands.UserJobDesc1 =  data.String,
                error: () => this.m_JobQBackend.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJobDesc2", Default: "User Job #2" })
            .subscribe({
                next: data => this.m_JobQCommands.UserJobDesc2 =  data.String,
                error: () => this.m_JobQCommands.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJobDesc3", Default: "User Job #3" })
            .subscribe({
                next: data => this.m_JobQCommands.UserJobDesc3 =  data.String,
                error: () => this.m_JobQCommands.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJobDesc4", Default: "User Job #4" })
            .subscribe({
                next: data => this.m_JobQCommands.UserJobDesc4 =  data.String,
                error: () => this.m_JobQCommands.errorCount++
            });
        return this.m_JobQCommands;
    }

    m_JobQGlobal!: JobQGlobal;

    getJobQGlobal(): JobQGlobal {
        this.m_JobQGlobal = {
            successCount: 0,
            errorCount: 0,
            JobsRunOnRecordHost:            false,
            AutoCommflagWhileRecording:     false,
            JobQueueCommFlagCommand:        "mythcommflag",
            JobQueueTranscodeCommand:       "mythtranscode",
            AutoTranscodeBeforeAutoCommflag:false,
            SaveTranscoding:                false,
        }

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "JobsRunOnRecordHost", Default: "0" })
            .subscribe({
                next: data => this.m_JobQGlobal.JobsRunOnRecordHost =  (data.String == '1'),
                error: () => this.m_JobQGlobal.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoCommflagWhileRecording", Default: "0" })
            .subscribe({
                next: data => this.m_JobQGlobal.AutoCommflagWhileRecording =  (data.String == '1'),
                error: () => this.m_JobQGlobal.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "JobQueueCommFlagCommand", Default: "mythcommflag" })
            .subscribe({
                next: data => this.m_JobQGlobal.JobQueueCommFlagCommand = data.String,
                error: () => this.m_JobQGlobal.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "JobQueueTranscodeCommand", Default: "mythtranscode" })
            .subscribe({
                next: data => this.m_JobQGlobal.JobQueueTranscodeCommand = data.String,
                error: () => this.m_JobQGlobal.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "AutoTranscodeBeforeAutoCommflag", Default: "0" })
            .subscribe({
                next: data => this.m_JobQGlobal.AutoTranscodeBeforeAutoCommflag =  (data.String == '1'),
                error: () => this.m_JobQGlobal.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "SaveTranscoding", Default: "0" })
            .subscribe({
                next: data => this.m_JobQGlobal.SaveTranscoding =  (data.String == '1'),
                error: () => this.m_JobQGlobal.errorCount++
            });
        return this.m_JobQGlobal;
    }


    JobQGlobal$ = {
        next: (x: any) => {
            if (x.bool)
                this.m_JobQGlobal.successCount++;
            else
                this.m_JobQGlobal.errorCount;
        },
        error: (err: any) => {
            console.error(err);
            this.m_JobQGlobal.errorCount++
        },
    };

    saveJobQGlobal() {
        this.m_JobQGlobal.successCount = 0;
        this.m_JobQGlobal.errorCount = 0;
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "JobsRunOnRecordHost",
            Value: this.m_JobQGlobal.JobsRunOnRecordHost ? "1" : "0"}).subscribe(this.JobQGlobal$);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "AutoCommflagWhileRecording",
            Value: this.m_JobQGlobal.AutoCommflagWhileRecording ? "1" : "0"}).subscribe(this.JobQGlobal$);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "JobQueueCommFlagCommand",
            Value: this.m_JobQGlobal.JobQueueCommFlagCommand}).subscribe(this.JobQGlobal$);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "JobQueueTranscodeCommand",
            Value: this.m_JobQGlobal.JobQueueTranscodeCommand}).subscribe(this.JobQGlobal$);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "AutoTranscodeBeforeAutoCommflag",
            Value: this.m_JobQGlobal.AutoTranscodeBeforeAutoCommflag ? "1" : "0"}).subscribe(this.JobQGlobal$);
        this.mythService.PutSetting({HostName: '_GLOBAL_', Key: "SaveTranscoding",
            Value: this.m_JobQGlobal.SaveTranscoding ? "1" : "0"}).subscribe(this.JobQGlobal$);
    }

}
