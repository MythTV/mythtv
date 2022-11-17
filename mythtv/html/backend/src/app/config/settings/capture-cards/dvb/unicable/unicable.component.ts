import { AfterViewInit, Component, EventEmitter, Input, OnInit, Output, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Observable, Observer, PartialObserver } from 'rxjs';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { DiseqcParm, DiseqcTree, DiseqcTreeList } from 'src/app/services/interfaces/capture-card.interface';
import { DiseqcSettingBase } from '../diseqc-setting-base';
import { DvbComponent } from '../dvb.component';

@Component({
  selector: 'app-unicable',
  templateUrl: './unicable.component.html',
  styleUrls: ['./unicable.component.css']
})
export class UnicableComponent implements OnInit, AfterViewInit, DiseqcSettingBase {

  @Input() diseqcTree!: DiseqcTree;
  @Input() diseqcTreeList!: DiseqcTreeList;
  @Input() dvbComponent!: DvbComponent;

  @ViewChild("unicableform")
  currentForm!: NgForm;

  diseqcSubComponent!: DiseqcSettingBase;
  selectedDiseqcType: DiseqcParm = { description: "", type: "", inactive: true };

  @Output() baseEvent = new EventEmitter<DiseqcSettingBase>();

  work = {
    displayNewDiseqc: false,
    displayDeleteDiseqc: false,
  }

  diseqcSubTree!: DiseqcTree | undefined;

  constructor(public captureCardService: CaptureCardService) { }

  ngOnInit(): void {

    if (!this.diseqcTree.DiSEqCId) {
      // New unicable - set defaults
      this.diseqcTree.ScrUserband = 0;
      this.diseqcTree.ScrFrequency = 1210;
      this.diseqcTree.ScrPin = -1;
      this.diseqcTree.CmdRepeat = 1;
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
            if (observer.next)
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
