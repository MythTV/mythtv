import { AfterViewInit, Component, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';

import { EpgDownload } from 'src/app/services/interfaces/setup.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-epg-downloading',
  templateUrl: './epg-downloading.component.html',
  styleUrls: ['./epg-downloading.component.css']
})

export class EpgDownloadingComponent implements OnInit, AfterViewInit {

  EpgDownloadData: EpgDownload = this.setupService.getEpgDownload();
  @ViewChild("epgdownload")
  currentForm!: NgForm;

  constructor(private setupService: SetupService) {
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
    this.setupService.saveEpgDownload(this.currentForm);
  }

}
