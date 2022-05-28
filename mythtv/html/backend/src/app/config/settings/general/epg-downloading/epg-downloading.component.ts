import { Component, OnInit } from '@angular/core';

import { EpgDownload } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-epg-downloading',
  templateUrl: './epg-downloading.component.html',
  styleUrls: ['./epg-downloading.component.css']
})

export class EpgDownloadingComponent implements OnInit {

  EpgDownloadData: EpgDownload = this.setupService.getEpgDownload();

  constructor(private setupService: SetupService) {
  }

  ngOnInit(): void {
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveForm() {
    console.log("save form clicked");
    this.setupService.saveEpgDownload();
  }

}
