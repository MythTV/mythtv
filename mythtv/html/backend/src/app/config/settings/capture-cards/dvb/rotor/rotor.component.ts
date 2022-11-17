import { AfterViewInit, Component, EventEmitter, Input, OnInit, Output, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { TranslateService } from '@ngx-translate/core';
import { Observable, Observer, PartialObserver } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { DiseqcParm, DiseqcTree, DiseqcTreeList } from 'src/app/services/interfaces/capture-card.interface';
import { MythService } from 'src/app/services/myth.service';
import { SetupService } from 'src/app/services/setup.service';
import { DiseqcSettingBase } from '../diseqc-setting-base';
import { DvbComponent } from '../dvb.component';

interface RotorType {
  Name: string;
  SubType: string;
}

interface Position {
  Num: number;
  Angle: number | null;
}

interface Fields {
  Latitude: null | number;
  Longitude: null | number;
}

@Component({
  selector: 'app-rotor',
  templateUrl: './rotor.component.html',
  styleUrls: ['./rotor.component.css']
})
export class RotorComponent implements OnInit, AfterViewInit, DiseqcSettingBase {

  @Input() diseqcTree!: DiseqcTree;
  @Input() diseqcTreeList!: DiseqcTreeList;
  @Input() dvbComponent!: DvbComponent;

  @ViewChild("rotorform")
  currentForm!: NgForm;
  diseqcSubComponent!: DiseqcSettingBase;

  @Output() baseEvent = new EventEmitter<DiseqcSettingBase>();

  rotorSubTypes: RotorType[] = [
    { Name: "settings.capture.diseqc.subtype_diseqc_1_2", SubType: "diseqc_1_2" },
    { Name: "settings.capture.diseqc.subtype_diseqc_1_3", SubType: "diseqc_1_3" }
  ]

  currentSubType!: RotorType;

  selectedDiseqcType: DiseqcParm = { description: "", type: "", inactive: true };

  rotorPositions: Position[] = [];

  fields: Fields = {
    Latitude: null,
    Longitude: null
  }

  work = {
    displayNewDiseqc: false,
    displayDeleteDiseqc: false,
  }

  diseqcSubTree!: DiseqcTree | undefined;

  constructor(public captureCardService: CaptureCardService, private setupService: SetupService,
    private translate: TranslateService, private mythService: MythService) {
    this.rotorSubTypes.forEach(x => translate.get(x.Name).subscribe(data => x.Name = data));
  }

  ngOnInit(): void {
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "latitude" })
      .subscribe({
        next: data => {
          if (data.String.length > 0)
            this.fields.Latitude = Number(data.String);
        },
        error: () => this.dvbComponent.work.errorCount++
      });
    this.mythService.GetSetting({ HostName: '_GLOBAL_', Key: "longitude" })
      .subscribe({
        next: data => {
          if (data.String.length > 0)
            this.fields.Longitude = Number(data.String)
        },
        error: () => this.dvbComponent.work.errorCount++
      });

    for (let ix = 0; ix < 48; ix++) {
      this.rotorPositions.push({ Num: ix + 1, Angle: null });
    }
    if (this.diseqcTree.DiSEqCId) {
      let entries = this.diseqcTree.RotorPositions.split(':');
      entries.forEach(x => {
        let fields = x.split('=');
        if (fields.length == 2 && fields[0].length > 0 && fields[1].length > 0) {
          let num = Number.parseInt(fields[1]) - 1;
          let angle = Number.parseFloat(fields[0]);
          if (num > -1 && num < 48)
            this.rotorPositions[num].Angle = angle;
        }
      })
    } else {
      // New rotor - set defaults
      this.diseqcTree.RotorHiSpeed = 2.5;
      this.diseqcTree.RotorLoSpeed = 1.9;
      this.diseqcTree.SubType = 'diseqc_1_3'
      this.diseqcTree.CmdRepeat = 1;
    }
    if (this.diseqcTree.SubType) {
      let subtype = this.rotorSubTypes.find(x => x.SubType == this.diseqcTree.SubType);
      if (subtype)
        this.currentSubType = subtype;
    }
    // Get sub component
    this.diseqcSubTree = this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.find
      (x => x.ParentId == this.diseqcTree.DiSEqCId);
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

  updateSubType(): void {
    this.diseqcTree.SubType = this.currentSubType.SubType;
  }

  newDiseqc(): void {
    this.work.displayNewDiseqc = false;
    this.diseqcSubTree = <DiseqcTree>{
      Type: this.selectedDiseqcType.type,
      Description: this.selectedDiseqcType.description,
    };
  }

  setDiseqcObject(base: DiseqcSettingBase): void {
    this.diseqcSubComponent = base;
  }

  deleteDiseqc(): void {
    this.work.displayDeleteDiseqc = false;
    this.dvbComponent.work.errorCount = 0;
    if (this.diseqcSubTree && this.diseqcSubTree.DiSEqCId) {
      this.captureCardService.DeleteDiseqcTree(this.diseqcSubTree.DiSEqCId)
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
    if (this.dvbComponent.work.errorCount == 0)
      this.diseqcSubTree = undefined;
  }

  saveForm(parent: number, observer: Observer<any>): void {

    this.diseqcTree.ParentId = parent;

    if (this.fields.Latitude != null) {
      this.mythService.PutSetting({
        HostName: "_GLOBAL_", Key: "latitude",
        Value: String(this.fields.Latitude)
      }).subscribe(this.dvbComponent.saveObserver);
    }

    if (this.fields.Longitude != null) {
      this.mythService.PutSetting({
        HostName: "_GLOBAL_", Key: "longitude",
        Value: String(this.fields.Longitude)
      }).subscribe(this.dvbComponent.saveObserver);
    }

    // Convert positions into string
    this.diseqcTree.RotorPositions = "";
    this.rotorPositions.forEach(x => {
      if (x.Angle != null) {
        if (this.diseqcTree.RotorPositions.length > 0)
          this.diseqcTree.RotorPositions = this.diseqcTree.RotorPositions + ":";
        this.diseqcTree.RotorPositions = this.diseqcTree.RotorPositions + x.Angle + "=" + x.Num;
      }
    })

    if (this.diseqcTree.DiSEqCId) {
      this.captureCardService.UpdateDiseqcTree(this.diseqcTree).subscribe(observer);
      // save sub tree
      if (this.diseqcSubComponent) {
        this.diseqcSubComponent.saveForm(this.diseqcTree.DiSEqCId, {
          error: (err: any) => {
            observer.error(err);
          }
        });
      }
    }
    else
      this.captureCardService.AddDiseqcTree(this.diseqcTree).subscribe({
        next: (x: any) => {
          if (x.int && x.int > 0) {
            this.diseqcTree.DiSEqCId = x.int;
            observer.next(x);
            // save sub tree
            if (this.diseqcSubComponent)
              this.diseqcSubComponent.saveForm(this.diseqcTree.DiSEqCId, {
                next: (x: any) => { },
                error: (err: any) => {
                  observer.error(err);
                }
              });
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
