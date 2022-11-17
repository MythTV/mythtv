import { AfterViewInit, Component, Input, OnInit, Output, ViewChild, EventEmitter } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { DiseqcTree, DiseqcTreeList } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';
import { DiseqcSettingBase } from '../diseqc-setting-base';
import { DvbComponent } from '../dvb.component';
import { Observable, Observer } from 'rxjs';

interface LnbPreset {
  Name: string;
  SubType: string;
  LnbLofSwitch: number;
  LnbLofLo: number;
  LnbLofHi: number;
  LnbPolInv: boolean;
}

interface LnbSubType {
  Name: string;
  SubType: string;
}

@Component({
  selector: 'app-lnb',
  templateUrl: './lnb.component.html',
  styleUrls: ['./lnb.component.css']
})
export class LnbComponent implements OnInit, AfterViewInit, DiseqcSettingBase {

  @Input() diseqcTree!: DiseqcTree;
  @Input() diseqcTreeList!: DiseqcTreeList;
  @Input() dvbComponent!: DvbComponent;

  @ViewChild("lnbform")
  currentForm!: NgForm;

  @Output() baseEvent = new EventEmitter<DiseqcSettingBase>();

  work = {
    // These are needed to hold the values in mHz
    // The database values are in kHz
    LnbLofSwitch: 0,
    LnbLofLo: 0,
    LnbLofHi: 0,
  }

  lnbSubTypes: LnbSubType[] = [
    { Name: "settings.capture.diseqc.subtype_legacy", SubType: "fixed" },
    { Name: "settings.capture.diseqc.subtype_standard", SubType: "voltage" },
    { Name: "settings.capture.diseqc.subtype_universal", SubType: "voltage_tone" },
    { Name: "settings.capture.diseqc.subtype_backstacked", SubType: "bandstacked" },
  ]

  currentSubType!: LnbSubType;

  lnbPresetList: LnbPreset[] = [
    {
      Name: "settings.capture.diseqc.lnbpreset_universal", SubType: "voltage_tone",
      LnbLofSwitch: 11700, LnbLofLo: 9750, LnbLofHi: 10600, LnbPolInv: false
    },
    {
      Name: "settings.capture.diseqc.lnbpreset_single", SubType: "voltage",
      LnbLofSwitch: 0, LnbLofLo: 9750, LnbLofHi: 0, LnbPolInv: false
    },
    {
      Name: "settings.capture.diseqc.lnbpreset_circular", SubType: "voltage",
      LnbLofSwitch: 0, LnbLofLo: 11250, LnbLofHi: 0, LnbPolInv: false
    },
    {
      Name: "settings.capture.diseqc.lnbpreset_Linear", SubType: "voltage",
      LnbLofSwitch: 0, LnbLofLo: 10750, LnbLofHi: 0, LnbPolInv: false
    },
    {
      Name: "settings.capture.diseqc.lnbpreset_cband", SubType: "voltage",
      LnbLofSwitch: 0, LnbLofLo: 5150, LnbLofHi: 0, LnbPolInv: false
    },
    {
      Name: "settings.capture.diseqc.lnbpreset_dishpro", SubType: "bandstacked",
      LnbLofSwitch: 0, LnbLofLo: 11250, LnbLofHi: 14350, LnbPolInv: false
    },
    {
      Name: "settings.capture.diseqc.lnbpreset_custom", SubType: "custom",
      LnbLofSwitch: 0, LnbLofLo: 0, LnbLofHi: 0, LnbPolInv: false
    },
  ];

  currentPreset!: LnbPreset;

  constructor(private captureCardService: CaptureCardService, private setupService: SetupService,
    private translate: TranslateService) {
    this.lnbSubTypes.forEach(x => translate.get(x.Name).subscribe(data => x.Name = data));
    this.lnbPresetList.forEach(x => translate.get(x.Name).subscribe(data => x.Name = data));
  }

  ngOnInit(): void {
    if (this.diseqcTree.DiSEqCId) {
      // For presentation divide by 1000 to convert to Mhz
      this.work.LnbLofSwitch = this.diseqcTree.LnbLofSwitch / 1000;
      this.work.LnbLofLo = this.diseqcTree.LnbLofLo / 1000;
      this.work.LnbLofHi = this.diseqcTree.LnbLofHi / 1000;
      let result: LnbPreset | undefined;
      result = this.lnbPresetList.find
        (x => x.SubType == this.diseqcTree.SubType
          && x.LnbLofSwitch == this.work.LnbLofSwitch
          && x.LnbLofLo == this.work.LnbLofLo
          && x.LnbLofHi == this.work.LnbLofHi
          && x.LnbPolInv == this.diseqcTree.LnbPolInv
        );
      // Assume that "Custom" is the last entry in preset list.
      if (result == undefined) {
        this.currentPreset = this.lnbPresetList[this.lnbPresetList.length - 1];
      }
      else
        this.currentPreset = result;
    }
    else {
      // Default entry 0 for new record.
      this.currentPreset = this.lnbPresetList[0];
      this.updatePreset();
    }
  }

  setupDone = false;
  ngAfterViewInit(): void {
    this.baseEvent.emit(this);
    this.currentForm.valueChanges!.subscribe(
      (x) => {
        if (this.setupDone && this.currentForm.dirty)
          this.dvbComponent.currentForm.form.markAsDirty()
      });
    let obs = new Observable(x => {
      setTimeout(() => {
        x.next(1);
        x.complete();
      }, 100)
    })
    obs.subscribe(x => {
      this.setupDone = true;
      if (this.diseqcTree.DiSEqCId) {
        this.currentForm.form.markAsPristine();
      } else {
        this.currentForm.form.markAsDirty()
        this.dvbComponent.currentForm.form.markAsDirty()
      }
    });
  }

  updatePreset(): void {
    if (this.currentPreset.SubType != 'custom') {
      let newSubType = this.lnbSubTypes.find(x => x.SubType == this.currentPreset.SubType);
      if (newSubType) {
        this.currentSubType = newSubType;
        this.updateSubType();
      }
      this.work.LnbLofSwitch = this.currentPreset.LnbLofSwitch;
      this.work.LnbLofLo = this.currentPreset.LnbLofLo;
      this.work.LnbLofHi = this.currentPreset.LnbLofHi;
      this.diseqcTree.LnbPolInv = this.currentPreset.LnbPolInv;
    }
  }

  updateSubType(): void {
    this.diseqcTree.SubType = this.currentSubType.SubType;
  }

  saveForm(parent: number, observer: Observer<any>): void {
    this.diseqcTree.ParentId = parent;
    // Convert values to mHz for saving
    this.diseqcTree.LnbLofSwitch = this.work.LnbLofSwitch * 1000;
    this.diseqcTree.LnbLofLo = this.work.LnbLofLo * 1000;
    this.diseqcTree.LnbLofHi = this.work.LnbLofHi * 1000;

    if (this.diseqcTree.DiSEqCId)
      this.captureCardService.UpdateDiseqcTree(this.diseqcTree).subscribe(observer);
    else {
      this.captureCardService.AddDiseqcTree(this.diseqcTree).subscribe({
        next: (x: any) => {
          if (x.int && x.int > 0) {
            this.diseqcTree.DiSEqCId = x.int;
            if (observer.next)
              observer.next(x);
          }
          else {
            console.log("UpdateDiseqcTree", x)
            observer.error(x);
          }
        },
        error: (err: any) => {
          console.log("UpdateDiseqcTree", err);
          observer.error(err);
        }
      });
    }
  }

}
