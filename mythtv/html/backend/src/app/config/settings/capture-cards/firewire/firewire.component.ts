import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CaptureDevice, CaptureDeviceList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-firewire',
  templateUrl: './firewire.component.html',
  styleUrls: ['./firewire.component.css']
})
export class FirewireComponent implements OnInit, AfterViewInit {
  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;

  @ViewChild("firewireform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  messages = {
    devNotExist: 'settings.capture.dvb.devNotExist',
    unknownName: 'settings.capture.dvb.unknownName',
    devInUse: 'settings.capture.dvb.devInUse',
    noDevSelected: 'settings.capture.dvb.noDevSelected',
    manuallyEnter: 'settings.capture.vbox.manuallyenter'
  };

  models = [
    { name: 'settings.capture.firewire.motogeneric', value: 'MOTO GENERIC' },
    { name: 'settings.capture.firewire.sageneric', value: 'SA GENERIC' },
    { name: 'DCH-3200', value: 'DCH-3200' },
    { name: 'DCX-3200', value: 'DCX-3200' },
    { name: 'DCT-3412', value: 'DCT-3412' },
    { name: 'DCT-3416', value: 'DCT-3416' },
    { name: 'DCT-6200', value: 'DCT-6200' },
    { name: 'DCT-6212', value: 'DCT-6212' },
    { name: 'DCT-6216', value: 'DCT-6216' },
    { name: 'QIP-6200', value: 'QIP-6200' },
    { name: 'QIP-7100', value: 'QIP-7100' },
    { name: 'PACE-550', value: 'PACE-550' },
    { name: 'PACE-779', value: 'PACE-779' },
    { name: 'SA3250HD', value: 'SA3250HD' },
    { name: 'SA4200HD', value: 'SA4200HD' },
    { name: 'SA4250HDC', value: 'SA4250HDC' },
    { name: 'SA8300HD', value: 'SA8300HD' },
  ];

  connectionTypes = [
    { name: 'settings.capture.firewire.pointtopoint', value: 0 },
    { name: 'settings.capture.firewire.broadcast', value: 1 }
  ];

  speeds = [
    { name: '100Mbps', value: 0 },
    { name: '200Mbps', value: 1 },
    { name: '400Mbps', value: 2 },
    { name: '800Mbps', value: 3 },
  ];

  captureDeviceList: CaptureDeviceList = {
    CaptureDeviceList: {
      CaptureDevices: [],
    }
  };

  currentDevice: CaptureDevice = <CaptureDevice>{ FrontendName: "Unknown", InputNames: [''] };

  isReady = false;
  warningMessage = '';
  errorCount = 0;
  successCount = 0;

  constructor(public captureCardService: CaptureCardService, public setupService: SetupService,
    private translate: TranslateService) {
    translate.get(this.messages.devNotExist).subscribe(data => this.messages.devNotExist = data);
    translate.get(this.messages.unknownName).subscribe(data => this.messages.unknownName = data);
    translate.get(this.messages.devInUse).subscribe(data => this.messages.devInUse = data);
    translate.get(this.messages.noDevSelected).subscribe(data => this.messages.noDevSelected = data);
    translate.get(this.messages.manuallyEnter).subscribe(data => this.messages.manuallyEnter = data);
    translate.get(this.models[0].name).subscribe(data => this.models[0].name = data);
    translate.get(this.models[1].name).subscribe(data => this.models[1].name = data);
    translate.get(this.connectionTypes[0].name).subscribe(data => this.connectionTypes[0].name = data);
    translate.get(this.connectionTypes[1].name).subscribe(data => this.connectionTypes[1].name = data);
  }

  ngOnInit(): void {
    // Get list of devices for dropdown list
    this.captureCardService.GetCaptureDeviceList('FIREWIRE')
      .subscribe({
        next: (data: CaptureDeviceList) => {
          this.captureDeviceList = data;
          this.setupDevice();
        },
        error: (err: any) => {
          console.log("GetCaptureDeviceList", err);
          this.errorCount++;
        }
      });

  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  // After load of devices, make sure the current record is selected in list
  setupDevice(): void {
    // Add one blank entry at the start if it is a new card
    // This is to prevent the system automaitically selecting the first entry
    // in the list when you add a new card
    if (!this.card.VideoDevice) {
      let dummy = <CaptureDevice>{
        VideoDevice: '',
        FrontendName: this.messages.noDevSelected,
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
        };
        this.captureDeviceList.CaptureDeviceList.CaptureDevices.push(this.currentDevice);
      }
    }
    this.isReady = true;
  }

  // After device update, update device-dependent fields
  updateDevice(): void {
    // Update device-dependent fields

    this.card.VideoDevice = this.currentDevice.VideoDevice;
    this.card.ChannelTimeout = this.currentDevice.ChannelTimeout;
    this.card.SignalTimeout = this.currentDevice.SignalTimeout;
    this.card.FirewireModel = this.currentDevice.FirewireModel;
    this.checkInUse();
  }

  checkInUse(): void {
    // Check if already in use
    let match = this.cardList.CaptureCardList.CaptureCards.find
      (x => x.VideoDevice == this.currentDevice.VideoDevice
        && x.CardId != this.card.CardId);
    if (match)
      this.warningMessage = this.messages.devInUse;
    else
      this.warningMessage = "";
  }


  // good response to add: {"int": 19}
  saveObserver = {
    next: (x: any) => {
      if (this.card.CardId && x.bool)
        this.successCount++;
      else if (!this.card.CardId && x.int) {
        this.successCount++;
        this.card.CardId = x.int;
        this.cardList.CaptureCardList.CaptureCards.push(this.card);
      }
      else {
        this.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.error(err);
      this.errorCount++;
      this.currentForm.form.markAsDirty();
    },
  };

  saveForm() {
    this.successCount = 0;
    this.errorCount = 0;
    if (this.card.CardId) {
      // Update device and child devices
      this.cardList.CaptureCardList.CaptureCards.forEach(card => {
        if (card.CardId == this.card.CardId || card.ParentId == this.card.CardId) {
          this.captureCardService.UpdateCaptureCard(card.CardId, 'videodevice', this.card.VideoDevice)
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'firewire_model', this.card.FirewireModel)
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'firewire_connection',
            String(this.card.FirewireConnection))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'firewire_speed',
            String(this.card.FirewireSpeed))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'signal_timeout',
            String(this.card.SignalTimeout))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'channel_timeout',
            String(this.card.ChannelTimeout))
            .subscribe(this.saveObserver);
        }
      });
    }
    else {
      this.captureCardService.AddCaptureCard(this.card).subscribe(this.saveObserver);
    }
  }

}
