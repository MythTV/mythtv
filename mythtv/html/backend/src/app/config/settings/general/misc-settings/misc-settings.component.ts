import { Component, OnInit } from '@angular/core';

import { Miscellaneous } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';


interface Scheduler {
  name: string,
  code: string
}

interface UpnpSource {
  name: string,
  code: string
}


@Component({
  selector: 'app-misc-settings',
  templateUrl: './misc-settings.component.html',
  styleUrls: ['./misc-settings.component.css']
})

export class MiscSettingsComponent implements OnInit {
  miscData: Miscellaneous = this.setupService.getMiscellaneousData();
  soptions: Scheduler[] = [
    {name: "Balanced free space", code: "BalancedFreeSpace" },
    {name: "Balanced percent free space", code: "BalancedPercFreeSpace"},
    {name: "Balanced disk I/O", code: "BalancedDiskIO"},
    {name: "Combination", code: "Combination"}
  ];
  uoptions: UpnpSource [] = [
    {name: "Recordings", code: "0" },
    {name: "Videos", code: "1"},
  ];

  constructor(private setupService: SetupService) { }

  ngOnInit(): void {
  }

  showHelp() {
      console.log("show help clicked");
      console.log(this);
  }

  saveForm() {
      console.log("save form clicked");
      this.setupService.saveMiscellaneousSettings();
  }
}
