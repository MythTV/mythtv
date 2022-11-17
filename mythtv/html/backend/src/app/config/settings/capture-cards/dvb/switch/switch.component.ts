import { AfterViewInit, Component, EventEmitter, Input, OnInit, Output, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, Observer } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { DiseqcParm, DiseqcTree, DiseqcTreeList } from 'src/app/services/interfaces/capture-card.interface';
import { DiseqcSettingBase } from '../diseqc-setting-base';
import { DvbComponent } from '../dvb.component';

interface SwitchType {
  Name: string;
  SubType: string;
}

@Component({
  selector: 'app-switch',
  templateUrl: './switch.component.html',
  styleUrls: ['./switch.component.css']
})
export class SwitchComponent implements OnInit, AfterViewInit, DiseqcSettingBase {

  @Input() diseqcTree!: DiseqcTree;
  @Input() diseqcTreeList!: DiseqcTreeList;
  @Input() dvbComponent!: DvbComponent;
  @ViewChild("switchform")
  currentForm!: NgForm;
  diseqcSubComponent: DiseqcSettingBase[] = [];

  @Output() baseEvent = new EventEmitter<DiseqcSettingBase>();

  switchSubTypes: SwitchType[] = [
    { Name: "settings.capture.diseqc.switch.tone", SubType: "tone" },
    { Name: "settings.capture.diseqc.switch.voltage", SubType: "voltage" },
    { Name: "settings.capture.diseqc.switch.mini_diseqc", SubType: "mini_diseqc" },
    { Name: "settings.capture.diseqc.switch.diseqc", SubType: "diseqc" },
    { Name: "settings.capture.diseqc.switch.diseqc_uncom", SubType: "diseqc_uncom" },
    { Name: "settings.capture.diseqc.switch.legacy_sw21", SubType: "legacy_sw21" },
    { Name: "settings.capture.diseqc.switch.legacy_sw42", SubType: "legacy_sw42" },
    { Name: "settings.capture.diseqc.switch.legacy_sw64", SubType: "legacy_sw64" },
  ]

  currentSubType!: SwitchType;

  diseqcSubTree: (DiseqcTree | null)[] = [];
  diseqcSubTreeCount = 0;

  work = {
    addressText: "",
    displayNewDiseqc: false,
    enableAddrAndPorts: false
  }
  displayDeleteThis: boolean[] = [];
  selectedDiseqcType: DiseqcParm = { description: "", type: "", inactive: true };

  constructor(public captureCardService: CaptureCardService, private translate: TranslateService) {
    this.switchSubTypes.forEach(x => translate.get(x.Name).subscribe(data => x.Name = data));
  }

  ngOnInit(): void {
    if (this.diseqcTree.DiSEqCId) {
      // convert address to hexadecimal
      this.work.addressText = "0x" + this.diseqcTree.Address.toString(16);
    } else {
      // default values
      this.diseqcTree.Address = 16;
      this.work.addressText = "0x10";
      this.diseqcTree.SwitchPorts = 2;
      this.diseqcTree.SubType = 'tone';
      this.diseqcTree.CmdRepeat = 1;
    }
    if (this.diseqcTree.SubType) {
      let subtype = this.switchSubTypes.find(x => x.SubType == this.diseqcTree.SubType);
      if (subtype)
        this.currentSubType = subtype;
    }
    this.updateSubType();
    // Populate sub-trees
    this.displayDeleteThis = [];
    this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.forEach
      (x => {
        if (x.ParentId == this.diseqcTree.DiSEqCId) {
          this.diseqcSubTree.push(x)
          this.displayDeleteThis.push(false);
          this.diseqcSubTreeCount++;
        }
      });
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

  newDiseqc(): void {
    this.work.displayNewDiseqc = false;
    this.diseqcSubTree.push(<DiseqcTree>{
      Type: this.selectedDiseqcType.type,
      Description: this.selectedDiseqcType.description,
    });
    this.displayDeleteThis.push(false);
    this.diseqcSubTreeCount++;
  }

  setDiseqcObject(object: any): void {
    let ix = this.diseqcSubComponent.findIndex(x => x === object);
    if (ix < 0)
      this.diseqcSubComponent.push(object);
    console.log("setDiseqcObject", this.diseqcSubComponent.length);
  }

  deleteDiseqc(ix: number): void {
    this.displayDeleteThis[ix] = false;
    this.dvbComponent.work.errorCount = 0;
    console.log("Delete", ix);
    if (this.diseqcSubTree[ix] == null)
      return;
    if (this.diseqcSubTree[ix]!.DiSEqCId) {
      this.captureCardService.DeleteDiseqcTree(this.diseqcSubTree[ix]!.DiSEqCId)
        .subscribe({
          next: (x: any) => {
            if (!x.bool) {
              console.log("DeleteDiseqcTree", x);
              this.dvbComponent.work.errorCount++;
            }
          },
          error: (err: any) => {
            console.log("DeleteDiseqcTree", err);
            this.dvbComponent.work.errorCount++;
          }
        });
    }
    if (this.dvbComponent.work.errorCount == 0) {
      this.diseqcSubTree[ix] = null;
      this.diseqcSubTreeCount--;
    }
  }


  updateSubType(): void {
    this.diseqcTree.SubType = this.currentSubType.SubType;
    switch (this.diseqcTree.SubType) {

      case "tone":
      case "voltage":
      case "mini_diseqc":
      case "legacy_sw21":
      case "legacy_sw42":
        this.work.addressText = "0x10";
        this.diseqcTree.SwitchPorts = 2;
        this.work.enableAddrAndPorts = false;
        break;

      case "legacy_sw64":
        this.work.addressText = "0x10";
        this.diseqcTree.SwitchPorts = 3;
        this.work.enableAddrAndPorts = false;
        break;

      case "diseqc_uncom":
      case "diseqc":
        this.work.enableAddrAndPorts = true;
        break;
    }
  }

  saveForm(parent: number, observer: Observer<any>): void {

    this.diseqcTree.ParentId = parent;

    // get address from hexadecimal text
    this.diseqcTree.Address = Number.parseInt(this.work.addressText);

    if (this.diseqcTree.DiSEqCId) {
      this.captureCardService.UpdateDiseqcTree(this.diseqcTree).subscribe(observer);
      // save sub trees
      this.diseqcSubComponent.forEach(x => {
        if (x)
          x.saveForm(this.diseqcTree.DiSEqCId, {
            error: (err: any) => {
              observer.error(err);
            }
          });
      });
    }
    else
      this.captureCardService.AddDiseqcTree(this.diseqcTree).subscribe({
        next: (x: any) => {
          if (x.int && x.int > 0) {
            this.diseqcTree.DiSEqCId = x.int;
            if (observer.next)
              observer.next(x);
            // save sub trees
            this.diseqcSubComponent.forEach(x => {
              if (x)
                x.saveForm(this.diseqcTree.DiSEqCId, {
                  next: (x: any) => { },
                  error: (err: any) => {
                    observer.error(err);
                  }
                });
            })
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
