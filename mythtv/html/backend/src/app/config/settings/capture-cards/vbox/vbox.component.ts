import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Subscription } from 'rxjs';
import { delay } from 'rxjs/operators';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CaptureDevice, CaptureDeviceList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-vbox',
  templateUrl: './vbox.component.html',
  styleUrls: ['./vbox.component.css']
})
export class VboxComponent implements OnInit, AfterViewInit {
  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;

  @ViewChild("vboxform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  messages = {
    devNotExist: 'settings.capture.dvb.devNotExist',
    unknownName: 'settings.capture.dvb.unknownName',
    devInUse: 'settings.capture.dvb.devInUse',
    noDevSelected: 'settings.capture.dvb.noDevSelected',
    manuallyEnter: 'settings.capture.vbox.manuallyenter'
  };

  captureDeviceList: CaptureDeviceList = {
    CaptureDeviceList: {
      CaptureDevices: [],
    }
  };

  currentDevice: CaptureDevice = <CaptureDevice>{ FrontendName: "Unknown", InputNames: [''] };

  manualDevice!: CaptureDevice;

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

  }

  ngOnInit(): void {
    // Get list of devices for dropdown list
    this.captureCardService.GetCaptureDeviceList('VBOX')
      .subscribe({
        next: data => {
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
    // Add "Manually Enter" option
    this.manualDevice = <CaptureDevice>{
      VideoDevicePrompt: this.messages.manuallyEnter,
      VideoDevice: '',
      Description: '',
      IPAddress: '',
      TunerNumber: 0,
      SignalTimeout: 7000,
      ChannelTimeout: 10000
    }
    this.captureDeviceList.CaptureDeviceList.CaptureDevices.unshift(this.manualDevice);

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
  subscription?: Subscription;
  updateDevice(): void {
    // Update device-dependent fields
    if (this.currentDevice === this.manualDevice) {
      this.subscription = this.currentForm.valueChanges!.pipe(delay(50)).subscribe(
        () => this.card.VideoDevice = this.manualDevice.IPAddress + '-' + this.manualDevice.TunerNumber);
    } else {
      if (this.subscription) {
        this.subscription.unsubscribe();
        this.subscription = undefined;
      }
    }

    this.card.VideoDevice = this.currentDevice.VideoDevice;
    this.card.ChannelTimeout = this.currentDevice.ChannelTimeout;
    this.card.SignalTimeout = this.currentDevice.SignalTimeout;
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
