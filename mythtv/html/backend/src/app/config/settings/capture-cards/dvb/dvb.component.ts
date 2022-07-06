import { AfterViewInit, Component, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CaptureDevice, CaptureDeviceList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-dvb',
  templateUrl: './dvb.component.html',
  styleUrls: ['./dvb.component.css']
})
export class DvbComponent implements OnInit, AfterViewInit {

  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;

  @ViewChild("dvbform")
  currentForm!: NgForm;

  work = {
    InputName: '',
    successCount: 0,
    errorCount: 0,
    inputNames: [''],
    isReady: false,
    warningMessage: ''
  };

  captureDeviceList: CaptureDeviceList = {
    CaptureDeviceList: {
      CaptureDevices: [],
    }
  };

  messages = {
    devNotExist: 'settings.capture.dvb.devNotExist',
    unknownName: 'settings.capture.dvb.unknownName',
    devInUse: 'settings.capture.dvb.devInUse',
    noDevSelected: 'settings.capture.dvb.noDevSelected',
  };

  currentDevice: CaptureDevice = <CaptureDevice>{ FrontendName: "Unknown", InputNames: [''] };

  constructor(private captureCardService: CaptureCardService, private setupService: SetupService,
    private translate: TranslateService) {
    translate.get(this.messages.devNotExist).subscribe(data => this.messages.devNotExist = data);
    translate.get(this.messages.unknownName).subscribe(data => this.messages.unknownName = data);
    translate.get(this.messages.devInUse).subscribe(data => this.messages.devInUse = data);
    translate.get(this.messages.noDevSelected).subscribe(data => this.messages.noDevSelected = data);
  }

  ngOnInit(): void {
    // initialize these in case of an "add card" request
    if (this.card.DVBWaitForSeqStart == undefined)
      this.card.DVBWaitForSeqStart = true;
    if (this.card.DVBOnDemand == undefined)
      this.card.DVBOnDemand = true;
    if (this.card.DVBEITScan == undefined)
      this.card.DVBEITScan = true;
    if (this.card.CardId == undefined)
      this.card.CardId = 0;
    if (this.card.VideoDevice == undefined)
      this.card.VideoDevice = '';
    // Get list of devices for dropdown liat
    this.captureCardService.GetCaptureDeviceList('DVB')
      .subscribe({
        next: data => {
          this.captureDeviceList = data;
          this.setupDevice();
        },
        error: () => this.work.errorCount++
      });
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
  }

  // After load of devices, make sure the current record is selected in list
  setupDevice(): void {
    // Add one blank entry at the start if it is a new card
    if (!this.card.VideoDevice) {
      let dummy = <CaptureDevice>{
        VideoDevice: '',
        FrontendName: this.messages.noDevSelected,
        InputNames: ['']
      }
      this.captureDeviceList.CaptureDeviceList.CaptureDevices.unshift(dummy);
    }
    if (this.card.VideoDevice) {
      let device = this.captureDeviceList.CaptureDeviceList.CaptureDevices.find(x => x.VideoDevice == this.card.VideoDevice);
      if (device)
        this.currentDevice = device;
      else {
        this.currentDevice = <CaptureDevice>{
          VideoDevice: this.card.VideoDevice,
          FrontendName: this.messages.devNotExist,
          InputNames: ['']
        };
        this.captureDeviceList.CaptureDeviceList.CaptureDevices.push(this.currentDevice);
      }
    }
    if (this.currentDevice && this.card.InputName) {
      if (!this.currentDevice.InputNames.includes(this.card.InputName)) {
        this.currentDevice.InputNames.push(this.card.InputName);
      }
    }
    this.work.isReady = true;
  }

  // After device update, update device-dependent fields
  updateDevice(): void {
    // Update device-dependent fields
    this.card.VideoDevice = this.currentDevice.VideoDevice;
    this.card.InputName = this.currentDevice.DefaultInputName;
    this.card.SignalTimeout = this.currentDevice.SignalTimeout;
    this.card.SignalTimeout = this.currentDevice.SignalTimeout;
    this.checkInUse();
  }

  checkInUse(): void {
    // Check if already in use
    let match = this.cardList.CaptureCardList.CaptureCards.find
      (x => x.VideoDevice == this.currentDevice.VideoDevice
        && x.CardId != this.card.CardId && x.ParentId != this.card.CardId);
    if (match)
      this.work.warningMessage = this.messages.devInUse;
    else
      this.work.warningMessage = "";
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  // good response to add: {"int": 19}
  saveObserver = {
    next: (x: any) => {
      if (this.card.CardId && x.bool)
        this.work.successCount++;
      else if (!this.card.CardId && x.int) {
        this.work.successCount++;
        if (!this.card.CardId) {
          this.card.CardId = x.int;
          this.cardList.CaptureCardList.CaptureCards.push(this.card);
        }
      }
      else {
        this.work.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.work.errorCount++;
      this.currentForm.form.markAsDirty();
    },
  };


  saveForm() {
    console.log("save form clicked");
    this.work.successCount = 0;
    this.work.errorCount = 0;
    if (this.card.CardId) {
      // Update device and child devices
      this.cardList.CaptureCardList.CaptureCards.forEach(card => {
        if (card.CardId == this.card.CardId || card.ParentId == this.card.CardId) {
          this.captureCardService.UpdateCaptureCard(card.CardId, 'videodevice', this.card.VideoDevice)
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'dvb_wait_for_seqstart',
            this.card.DVBWaitForSeqStart ? '1' : '0')
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'dvb_on_demand',
            this.card.DVBOnDemand ? '1' : '0')
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'signal_timeout',
            String(this.card.SignalTimeout))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'channel_timeout',
            String(this.card.ChannelTimeout))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'dvb_tuning_delay',
            String(this.card.DVBTuningDelay))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'dvb_eitscan',
            String(this.card.DVBEITScan ? '1' : '0'))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'inputname',
            String(this.card.InputName))
            .subscribe(this.saveObserver);
        }
      });
    }
    else {
      this.captureCardService.AddCaptureCard(this.card).subscribe(this.saveObserver);
    }
  }

}
