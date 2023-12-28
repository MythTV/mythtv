import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';

import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';


@Component({
    selector: 'app-misc-settings',
    templateUrl: './misc-settings.component.html',
    styleUrls: ['./misc-settings.component.css']
})

export class MiscSettingsComponent implements OnInit, AfterViewInit {

    successCount = 0;
    errorCount = 0;
    MasterBackendOverride = false;
    DeletesFollowLinks = false;
    TruncateDeletesSlowly = false;
    HDRingbufferSize = 9400;
    StorageScheduler = "BalancedFreeSpace";
    UPNPWmpSource = "0";
    MiscStatusScript = "";
    DisableAutomaticBackup = false;
    DisableFirewireReset = false;
    hostName = '';

    soptions = [
        { name: 'settings.misc.sg_balfree', code: "BalancedFreeSpace" },
        { name: 'settings.misc.sg_balpercent', code: "BalancedPercFreeSpace" },
        { name: 'settings.misc.bal_io', code: "BalancedDiskIO" },
        { name: 'settings.misc.sg_combination', code: "Combination" }
    ];
    uoptions = [
        { name: 'settings.misc.upnp_recs', code: "0" },
        { name: 'settings.misc.upnp_videos', code: "1" },
    ];

    @ViewChild("miscsettings")
    currentForm!: NgForm;

    constructor(public setupService: SetupService, private translate: TranslateService,
        private mythService: MythService) {
        translate.get(this.soptions[0].name).subscribe(data => this.soptions[0].name = data);
        translate.get(this.soptions[1].name).subscribe(data => this.soptions[1].name = data);
        translate.get(this.soptions[2].name).subscribe(data => this.soptions[2].name = data);
        translate.get(this.soptions[3].name).subscribe(data => this.soptions[3].name = data);
        translate.get(this.uoptions[0].name).subscribe(data => this.uoptions[0].name = data);
        translate.get(this.uoptions[1].name).subscribe(data => this.uoptions[1].name = data);
        this.mythService.GetHostName().subscribe({
            next: data => {
              this.hostName = data.String;
              this.getMiscellaneousData();
            },
            error: () => this.errorCount++
          })
    }

    getMiscellaneousData() {

        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "MasterBackendOverride", Default: "0" })
            .subscribe({
                next: data => this.MasterBackendOverride = (data.String == "1"),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DeletesFollowLinks", Default: "0" })
            .subscribe({
                next: data => this.DeletesFollowLinks = (data.String == "1"),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.hostName, Key: "TruncateDeletesSlowly", Default: "0" })
            .subscribe({
                next: data => this.TruncateDeletesSlowly = (data.String == "1"),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "HDRingbufferSize", Default: "9400" })
            .subscribe({
                next: data => this.HDRingbufferSize = Number(data.String),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "StorageScheduler", Default: "BalancedFreeSpace" })
            .subscribe({
                next: data => this.StorageScheduler = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "UPNPWmpSource", Default: "0" })
            .subscribe({
                next: data => this.UPNPWmpSource = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.hostName, Key: "MiscStatusScript", Default: "" })
            .subscribe({
                next: data => this.MiscStatusScript = data.String,
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "DisableAutomaticBackup", Default: "0" })
            .subscribe({
                next: data => this.DisableAutomaticBackup = (data.String == "1"),
                error: () => this.errorCount++
            });
        this.mythService.GetSetting({ HostName: this.hostName, Key: "DisableFirewireReset", Default: "0" })
            .subscribe({
                next: data => this.DisableFirewireReset = (data.String == "1"),
                error: () => this.errorCount++
            });
    }


    ngOnInit(): void {
    }

    ngAfterViewInit() {
        this.setupService.setCurrentForm(this.currentForm);
    }


    miscObserver = {
        next: (x: any) => {
            if (x.bool)
                this.successCount++;
            else {
                this.errorCount++;
                if (this.currentForm)
                    this.currentForm.form.markAsDirty();
            }
        },
        error: (err: any) => {
            console.error(err);
            this.errorCount++
            if (this.currentForm)
                this.currentForm.form.markAsDirty();
        }
    };


    saveForm() {
        this.successCount = 0;
        this.errorCount = 0;

        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "MasterBackendOverride",
            Value: this.MasterBackendOverride ? "1" : "0"
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "DeletesFollowLinks",
            Value: this.DeletesFollowLinks ? "1" : "0"
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: this.hostName, Key: "TruncateDeletesSlowly",
            Value: this.TruncateDeletesSlowly ? "1" : "0"
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "HDRingbufferSize",
            Value: String(this.HDRingbufferSize)
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "StorageScheduler",
            Value: this.StorageScheduler
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "UPNPWmpSource",
            Value: this.UPNPWmpSource
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: this.hostName, Key: "MiscStatusScript",
            Value: this.MiscStatusScript
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: '_GLOBAL_', Key: "DisableAutomaticBackup",
            Value: this.DisableAutomaticBackup ? "1" : "0"
        })
            .subscribe(this.miscObserver);
        this.mythService.PutSetting({
            HostName: this.hostName, Key: "DisableFirewireReset",
            Value: this.DisableFirewireReset ? "1" : "0"
        })
            .subscribe(this.miscObserver);
    }

}
