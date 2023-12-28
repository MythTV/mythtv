import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { PartialObserver } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CaptureDevice, CaptureDeviceList, CardAndInput, DiseqcParm, DiseqcTree, DiseqcTreeList } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';
import { CaptureCardsComponent } from '../capture-cards.component';
import { DiseqcSettingBase } from './diseqc-setting-base';

@Component({
  selector: 'app-dvb',
  templateUrl: './dvb.component.html',
  styleUrls: ['./dvb.component.css']
})
export class DvbComponent implements OnInit, AfterViewInit {

  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;
  @Input() diseqcTreeList!: DiseqcTreeList;
  @Input() parent!: CaptureCardsComponent;

  @ViewChild("dvbform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  diseqcComponent!: DiseqcSettingBase;

  work = {
    InputName: '',
    successCount: 0,
    errorCount: 0,
    inputNames: [''],
    isReady: false,
    warningMessage: '',
    displayNewDiseqc: false,
    displayDeleteDiseqc: false,
    displayConnectDevice: false,
    displayDisconnect: false,
    selectedDiseqcTree: 0,
    diseqcError: false,
  };

  captureDeviceList: CaptureDeviceList = {
    CaptureDeviceList: {
      CaptureDevices: [],
    }
  };

  diseqcRootTrees: DiseqcTree[] = [];

  diseqcTree: DiseqcTree | undefined = undefined;

  messages = {
    devNotExist: 'settings.capture.dvb.devNotExist',
    unknownName: 'settings.capture.dvb.unknownName',
    devInUse: 'settings.capture.dvb.devInUse',
    noDevSelected: 'settings.capture.dvb.noDevSelected',
  };

  currentDevice: CaptureDevice = <CaptureDevice>{ FrontendName: "Unknown", InputNames: [''] };

  selectedDiseqcType: DiseqcParm = { description: "", type: "", inactive: true };

  constructor(public captureCardService: CaptureCardService, public setupService: SetupService,
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
    if (this.card.DiSEqCId == undefined)
      this.card.DiSEqCId = 0;
    // Get list of devices for dropdown list
    this.captureCardService.GetCaptureDeviceList('DVB')
      .subscribe({
        next: data => {
          this.captureDeviceList = data;
          this.setupDevice();
        },
        error: (err: any) => {
          console.log("GetCaptureDeviceList", err);
          this.work.errorCount++;
        }
      });
    this.setupDiseqc();
  }

  ngAfterViewInit(): void {
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  setupDiseqc(): void {
    this.diseqcTree = this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.find
      (x => x.DiSEqCId == this.card.DiSEqCId);
    this.work.diseqcError = false;
  }

  newDiseqc(): void {
    this.work.displayNewDiseqc = false;
    this.work.diseqcError = false;
    this.diseqcTree = <DiseqcTree>{
      Type: this.selectedDiseqcType.type,
      Description: this.selectedDiseqcType.description,
    };
  }

  setupRootTrees() {
    this.diseqcRootTrees = this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.filter(x => !x.ParentId)
  }

  selectDiseqc() {
    this.work.displayConnectDevice = false;
    this.work.diseqcError = false;
    this.card.DiSEqCId = this.work.selectedDiseqcTree;
    this.currentForm.form.markAsDirty();
    this.setupDiseqc();
  }

  unSelectDiseqc() {
    this.work.displayDisconnect = false;
    this.work.diseqcError = false;
    this.card.DiSEqCId = 0;
    this.currentForm.form.markAsDirty();
    this.diseqcTree = undefined;
    // Do not check results of this - it may fail if diseqc config was never set
    // up for this card.
    this.captureCardService.DeleteDiseqcConfig(this.card.CardId)
      .subscribe();
  }

  canDeleteDiseqc() {
    this.work.diseqcError = false;
    const cards = this.cardList.CaptureCardList.CaptureCards.filter(x => x.DiSEqCId == this.card.DiSEqCId);
    if (cards.length > 1)
      this.work.diseqcError = true;
    else
      this.work.displayDeleteDiseqc = true
  }

  deleteDiseqc(): void {
    this.work.displayDeleteDiseqc = false;
    this.work.successCount = 0;
    this.work.errorCount = 0;
    if (this.diseqcTree && this.diseqcTree.DiSEqCId) {
      this.captureCardService.DeleteDiseqcTree(this.diseqcTree.DiSEqCId)
        .subscribe({
          next: (x: any) => {
            if (x.bool)
              this.work.successCount++;
            else {
              this.work.errorCount++;
              console.log("DeleteDiseqcTree", x)
            }
            // reload lists
            this.parent.loadDiseqc();
          },
          error: (err: any) => {
            console.log("DeleteDiseqcTree", err);
            this.work.errorCount++;
          }
        });
      this.card.DiSEqCId = 0;
      if (this.card.CardId) {
        // Update device and child devices
        this.cardList.CaptureCardList.CaptureCards.forEach(card => {
          if (card.CardId == this.card.CardId || card.ParentId == this.card.CardId) {
            this.captureCardService.UpdateCaptureCard(card.CardId, 'DiSEqCId',
              String(this.card.DiSEqCId))
              .subscribe(this.saveObserver);
          }
        });
        // Do not check results of this - it may fail if diseqc config was never set
        // up for this card.
        this.captureCardService.DeleteDiseqcConfig(this.card.CardId)
          .subscribe();
      }
    }
    this.diseqcTree = undefined;
  }


  setDiseqcObject(base: DiseqcSettingBase): void {
    this.diseqcComponent = base;
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
    this.card.ChannelTimeout = this.currentDevice.ChannelTimeout;
    this.checkInUse();
  }

  checkInUse(): void {
    // Check if already in use
    let match = this.cardList.CaptureCardList.CaptureCards.find
      (x => x.VideoDevice == this.currentDevice.VideoDevice
        && x.CardId != this.card.CardId);
    if (match)
      this.work.warningMessage = this.messages.devInUse;
    else
      this.work.warningMessage = "";
  }

  // good response to add: {"int": 19}
  saveObserver: PartialObserver<any> = {
    next: (x: any) => {
      if (x.bool) {
        console.log("saveObserver success", x);
        this.work.successCount++;
      }
      else if (!this.card.CardId && x.int) {
        console.log("saveObserver success", x);
        this.work.successCount++;
        if (!this.card.CardId) {
          this.card.CardId = x.int;
          this.cardList.CaptureCardList.CaptureCards.push(this.card);
          // Due to an omission in the AddCaptureCard service, inputname is not added
          // at add time, so update it here.
          this.captureCardService.UpdateCaptureCard(this.card.CardId, 'inputname',
            this.card.InputName)
            .subscribe(this.saveObserver);
        }
      }
      else {
        console.log("saveObserver error", x);
        this.work.errorCount++;
        this.currentForm.form.markAsDirty();
      }
    },
    error: (err: any) => {
      console.log("saveObserver error", err);
      this.work.errorCount++;
      this.currentForm.form.markAsDirty();
    }
  };


  saveForm() {
    this.work.successCount = 0;
    this.work.errorCount = 0;
    this.work.diseqcError = false;
    //  Save diseqcTree
    if (this.diseqcTree && this.diseqcComponent) {
      this.diseqcComponent.saveForm(0, {
        next: (x: any) => {
          if (this.card.DiSEqCId && x.bool
            && this.card.DiSEqCId == this.diseqcTree?.DiSEqCId) {
            this.work.successCount++;
            this.saveCard();
          }
          else if (x.int && this.diseqcTree) {
            this.card.DiSEqCId = x.int;
            this.diseqcTree.DiSEqCId = x.int;
            this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.push(this.diseqcTree);
            this.saveCard();
          }
          else {
            console.log("saveForm", x);
            this.work.errorCount++;
            this.currentForm.form.markAsDirty();
          }
        },
        error: (err: any) => {
          console.log("saveForm", err);
          this.work.errorCount++;
          this.currentForm.form.markAsDirty();
        },
        complete: () => { }
      });
    }
    else
      this.saveCard();
  }

  saveCard() {
    if (this.card.CardId) {
      // Update device and child devices
      this.cardList.CaptureCardList.CaptureCards.forEach(card => {
        if (card.CardId == this.card.CardId || card.ParentId == this.card.CardId) {
          this.captureCardService.UpdateCaptureCard(card.CardId, 'videodevice',
            this.card.VideoDevice)
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
            this.card.InputName)
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'DiSEqCId',
            String(this.card.DiSEqCId))
            .subscribe(this.saveObserver);
        }
      });
    }
    else {
      this.captureCardService.AddCaptureCard(this.card).subscribe(this.saveObserver);
    }
  }

}
