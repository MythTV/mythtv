import { AfterViewInit, Component, EventEmitter, Input, OnInit, Output, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { Observer, PartialObserver } from 'rxjs';
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

    if (!this.diseqcTree.DiseqcId) {
      // New unicable - set defaults
      this.diseqcTree.ScrUserband = 0;
      this.diseqcTree.ScrFrequency = 1210;
      this.diseqcTree.ScrPin = -1;
      this.diseqcTree.CmdRepeat = 1;
    }

    // Get sub component
    this.diseqcSubTree = this.diseqcTreeList.DiseqcTreeList.DiseqcTrees.find
      (x => x.ParentId == this.diseqcTree.DiseqcId);

  }

  ngAfterViewInit(): void {
    this.baseEvent.emit(this);
    if (!this.diseqcTree.DiseqcId) {
      this.currentForm.form.markAsDirty()
      this.dvbComponent.currentForm.form.markAsDirty()
    }
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
    if (this.diseqcSubTree && this.diseqcSubTree.DiseqcId) {
      this.captureCardService.DeleteDiseqcTree(this.diseqcSubTree.DiseqcId)
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

    if (this.diseqcTree.DiseqcId) {
      this.captureCardService.UpdateDiseqcTree(this.diseqcTree).subscribe(observer);
      // save sub tree
      if (this.diseqcSubComponent) {
        this.diseqcSubComponent.saveForm(this.diseqcTree.DiseqcId, {
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
            this.diseqcTree.DiseqcId = x.int;
            if (observer.next)
              observer.next(x);
            // save sub tree
            if (this.diseqcSubComponent)
              this.diseqcSubComponent.saveForm(this.diseqcTree.DiseqcId, {
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
