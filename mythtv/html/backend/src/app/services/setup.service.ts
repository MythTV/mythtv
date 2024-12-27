import { Injectable } from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

import {
    HostAddress, Locale, Setup,
    JobQCommands
}
    from './interfaces/setup.interface';
import { MythService } from './myth.service';
import { NgForm } from '@angular/forms';

@Injectable({
    providedIn: 'root'
})
export class SetupService {

    m_hostName: string = ""; // hostname of the backend server
    m_initialized: boolean = false;
    requesterForm: NgForm | null = null;


    constructor(private mythService: MythService, private translate: TranslateService) {
    }

    Init(): void {
        this.m_initialized = true;
    }

    m_HostAddressData: HostAddress = {
        successCount: 0,
        errorCount: 0,
        thisHostName:  this.m_hostName,
        BackendServerPort: 6543,
        BackendStatusPort: 6544,
        SecurityPin: '0000',
        AllowConnFromAll: false,
        ListenOnAllIps: true,
        BackendServerIP: '127.0.0.1',
        BackendServerIP6: '::1',
        AllowLinkLocal: true,
        BackendServerAddr: '',
        IsMasterBackend: true,
        MasterServerName: this.m_hostName,
    };

    getHostAddressData(): HostAddress {
        this.mythService.GetHostName().subscribe({
            next: data => {
                this.m_hostName = data.String;
                this.m_HostAddressData.thisHostName = this.m_hostName;
                this.m_HostAddressData.MasterServerName = this.m_hostName;
                this.getHostSettings();
            },
            error: () => this.m_HostAddressData.errorCount++
        })
        return this.m_HostAddressData;
    }

    getHostSettings () {
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerPort", Default:"6543" })
            .subscribe({
                next: data => this.m_HostAddressData.BackendServerPort = Number(data.String),
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendStatusPort", Default: "6544" })
            .subscribe({
                next: data => this.m_HostAddressData.BackendStatusPort = Number(data.String),
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "SecurityPin", Default: "0000"})
            .subscribe({
                next: data => this.m_HostAddressData.SecurityPin = data.String,
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "AllowConnFromAll", Default: "0" })
            .subscribe({
                next: data => this.m_HostAddressData.AllowConnFromAll = (data.String == "1"),
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "ListenOnAllIps", Default: "1" })
            .subscribe({
                next: data => this.m_HostAddressData.ListenOnAllIps = (data.String == "1"),
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerIP" })
            .subscribe({
                next: data => this.m_HostAddressData.BackendServerIP = data.String,
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerIP6" })
            .subscribe({
                next: data => this.m_HostAddressData.BackendServerIP6 = data.String,
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "UseLinkLocal" })
            .subscribe({
                next: data => this.m_HostAddressData.AllowLinkLocal = (data.String == "1"),
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.m_hostName, Key: "BackendServerAddr" })
            .subscribe({
                next: data => this.m_HostAddressData.BackendServerAddr = data.String,
                error: () => this.m_HostAddressData.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MasterServerName", Default: this.m_hostName })
            .subscribe({
                next: data => {
                    this.m_HostAddressData.MasterServerName = data.String;
                    this.m_HostAddressData.IsMasterBackend = (this.m_HostAddressData.MasterServerName == this.m_hostName);
                },
                error: () => this.m_HostAddressData.errorCount++
            });

    }

    HostAddressObs = {
        next: (x: any) => {
            if (x.bool)
                this.m_HostAddressData.successCount++;
            else {
                this.m_HostAddressData.errorCount++;
                if (this.requesterForm)
                    this.requesterForm.form.markAsDirty();
            }
        },
        error: (err: any) => {
            console.error(err);
            this.m_HostAddressData.errorCount++
            if (this.requesterForm)
                this.requesterForm.form.markAsDirty();
        }
    };

    saveHostAddressData(requesterForm: NgForm | null) {
        this.requesterForm = requesterForm;
        this.m_HostAddressData.successCount = 0;
        this.m_HostAddressData.errorCount = 0;
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "BackendServerPort",
            Value: String(this.m_HostAddressData.BackendServerPort)
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "BackendStatusPort",
            Value: String(this.m_HostAddressData.BackendStatusPort)
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "SecurityPin",
            Value: this.m_HostAddressData.SecurityPin
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "AllowConnFromAll",
            Value: this.m_HostAddressData.AllowConnFromAll ? "1" : "0"
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "ListenOnAllIps",
            Value: this.m_HostAddressData.ListenOnAllIps ? "1" : "0"
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "BackendServerIP",
            Value: this.m_HostAddressData.BackendServerIP
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "BackendServerIP6",
            Value: this.m_HostAddressData.BackendServerIP6
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "AllowLinkLocal",
            Value: this.m_HostAddressData.AllowLinkLocal ? "1" : "0"
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: this.m_hostName, Key: "BackendServerAddr",
            Value: this.m_HostAddressData.BackendServerAddr
        }).subscribe(this.HostAddressObs);
        this.mythService.PutSetting({
            HostName: "_GLOBAL_", Key: "MasterServerName",
            Value: this.m_HostAddressData.MasterServerName
        }).subscribe(this.HostAddressObs);

        if (this.m_HostAddressData.MasterServerName == this.m_HostAddressData.thisHostName) {
            // Update deprecated settings for apps that still use them
            this.mythService.PutSetting({
                HostName: "_GLOBAL_", Key: "MasterServerIP",
                Value: this.m_HostAddressData.BackendServerAddr
            }).subscribe(this.HostAddressObs);
            this.mythService.PutSetting({
                HostName: "_GLOBAL_", Key: "MasterServerPort",
                Value: String(this.m_HostAddressData.BackendServerPort)
            }).subscribe(this.HostAddressObs);
        }
    }

    m_LocaleData!: Locale;

    getLocaleData(): Locale {
        this.m_LocaleData = {
            successCount: 0,
            errorCount: 0,
            TVFormat: 'PAL',
            VbiFormat: 'None',
            FreqTable: 'us-bcast'
        }
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "TVFormat" })
            .subscribe({
                next: data => this.m_LocaleData.TVFormat = data.String,
                error: () => this.m_LocaleData.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "VbiFormat" })
            .subscribe({
                next: data => this.m_LocaleData.VbiFormat = data.String,
                error: () => this.m_LocaleData.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "FreqTable" })
            .subscribe({
                next: data => this.m_LocaleData.FreqTable = data.String,
                error: () => this.m_LocaleData.errorCount++
            });
        return this.m_LocaleData;
    }

    // Setup combines the first two sets, HostAddress and Locale

    m_setupData!: Setup;

    getSetupData(): Setup {
        this.getHostAddressData();
        this.getLocaleData();
        this.m_setupData = {
            General: {
                HostAddress: this.m_HostAddressData,
                Locale: this.m_LocaleData
            }
        }
        return this.m_setupData;
    }

    parseTime(date: Date, string: String) {
        let parts = string.split(':');
        date.setHours(Number(parts[0]));
        date.setMinutes(Number(parts[1]));
    }

    formatTime(date: Date): string {
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

    m_JobQCommands!: JobQCommands;

    getJobQCommands(): JobQCommands {
        // if already loaded, simply return the existing object.
        // This is different from others since it is used by more than one tab
        // and must not be reloaded for the other tab, as that may overwrite
        // unsaved changes.
        if (typeof this.m_JobQCommands == 'object')
            return this.m_JobQCommands;

        this.m_JobQCommands = {
            successCount: 0,
            errorCount: 0,
            UserJobDesc: [],
            UserJob: []
        }
        for (let ix = 0; ix < 4; ix++) {
            let num = ix + 1;
            this.translate.get('settings.services.job_default', { num: num })
                .subscribe(data => this.m_JobQCommands.UserJobDesc[ix] = data);
            this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJobDesc" + num, Default: "" })
                .subscribe({
                    next: data => {
                        if (data.String && data.String.length > 0)
                            this.m_JobQCommands.UserJobDesc[ix] = data.String;
                    },
                    error: () => this.m_JobQCommands.errorCount++
                });
            this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UserJob" + num, Default: "" })
                .subscribe({
                    next: data => this.m_JobQCommands.UserJob[ix] = data.String,
                    error: () => this.m_JobQCommands.errorCount++
                });
        }
        return this.m_JobQCommands;
    }

    JobQCommandsObs = {
        next: (x: any) => {
            if (x.bool)
                this.m_JobQCommands.successCount++;
            else {
                this.m_JobQCommands.errorCount++;
                if (this.requesterForm)
                    this.requesterForm.form.markAsDirty();
            }
        },
        error: (err: any) => {
            console.error(err);
            this.m_JobQCommands.errorCount++;
            if (this.requesterForm)
                this.requesterForm.form.markAsDirty();
        },
    };

    saveJobQCommands(requesterForm: NgForm | null) {
        this.requesterForm = requesterForm;
        this.m_JobQCommands.successCount = 0;
        this.m_JobQCommands.errorCount = 0;
        for (let ix = 0; ix < 4; ix++) {
            let num = ix + 1;
            this.mythService.PutSetting({
                HostName: '_GLOBAL_', Key: "UserJobDesc" + num,
                Value: this.m_JobQCommands.UserJobDesc[ix]
            }).subscribe(this.JobQCommandsObs);
            this.mythService.PutSetting({
                HostName: '_GLOBAL_', Key: "UserJob" + num,
                Value: this.m_JobQCommands.UserJob[ix]
            }).subscribe(this.JobQCommandsObs);
        }
    }

    currentForm: NgForm | null = null;

    getCurrentForm(): NgForm | null {
        return this.currentForm;
    }

    setCurrentForm(form: NgForm | null) {
        this.currentForm = form;
    }

    // This is here to be shared among tabs on an accordian
    schedulingEnabled = true;
    isDatabaseIgnored = false;
    DBTimezoneSupport = false;
    WebOnlyStartup = '';
    // pageType: Setup: 'S', Dashboard: 'D'
    pageType = '';
}
