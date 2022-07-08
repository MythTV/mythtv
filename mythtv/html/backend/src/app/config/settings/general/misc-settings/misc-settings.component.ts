import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';

import { Miscellaneous } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';


interface ddParam {
  name: string,
  code: string
}

@Component({
  selector: 'app-misc-settings',
  templateUrl: './misc-settings.component.html',
  styleUrls: ['./misc-settings.component.css']
})

export class MiscSettingsComponent implements OnInit, AfterViewInit {
  miscData: Miscellaneous = this.setupService.getMiscellaneousData();
  soptions: ddParam[] = [
    {name: 'settings.misc.sg_balfree', code: "BalancedFreeSpace" },
    {name: 'settings.misc.sg_balpercent', code: "BalancedPercFreeSpace"},
    {name: 'settings.misc.bal_io', code: "BalancedDiskIO"},
    {name: 'settings.misc.sg_combination', code: "Combination"}
  ];
  uoptions: ddParam [] = [
    {name: 'settings.misc.upnp_recs', code: "0" },
    {name: 'settings.misc.upnp_videos', code: "1"},
  ];

  @ViewChild("miscsettings")
  currentForm!: NgForm;

constructor(private setupService: SetupService, private translate: TranslateService) {
    translate.get(this.soptions[0].name).subscribe(data => this.soptions[0].name = data);
    translate.get(this.soptions[1].name).subscribe(data => this.soptions[1].name = data);
    translate.get(this.soptions[2].name).subscribe(data => this.soptions[2].name = data);
    translate.get(this.soptions[3].name).subscribe(data => this.soptions[3].name = data);
    translate.get(this.uoptions[0].name).subscribe(data => this.uoptions[0].name = data);
    translate.get(this.uoptions[1].name).subscribe(data => this.uoptions[1].name = data);
  }

  ngOnInit(): void {
  }

  ngAfterViewInit() {
    this.setupService.setCurrentForm(this.currentForm);
  }

  showHelp() {
      console.log("show help clicked");
      console.log(this);
  }

  saveForm() {
      console.log("save form clicked");
      this.setupService.saveMiscellaneousSettings(this.currentForm);
  }
}
