import { AfterViewInit, Component, ElementRef, Input, OnInit, ViewChild } from '@angular/core';
import { NgForm } from '@angular/forms';
import { delay } from 'rxjs/operators';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { SetupService } from 'src/app/services/setup.service';

@Component({
  selector: 'app-ceton',
  templateUrl: './ceton.component.html',
  styleUrls: ['./ceton.component.css']
})

export class CetonComponent implements OnInit, AfterViewInit {

  @Input() card!: CardAndInput;
  @Input() cardList!: CaptureCardList;
  @ViewChild("cetonform") currentForm!: NgForm;
  @ViewChild("top") topElement!: ElementRef;

  work = {
    ipAddress: '',
    tuner: '',
    successCount: 0,
    errorCount: 0,
  };

  constructor(private captureCardService: CaptureCardService, public setupService: SetupService) {
  }

  ngOnInit(): void {
    if (this.card.VideoDevice) {
      const parts = this.card.VideoDevice.split('-');
      if (parts.length == 2) {
        this.work.ipAddress = parts[0];
        const tparts = parts[1].split('.');
        if (tparts.length == 2)
          this.work.tuner = tparts[1];
      }
    }
  }

  ngAfterViewInit(): void {
    this.currentForm.valueChanges!.pipe(delay(50)).subscribe(
      () => this.card.VideoDevice = this.work.ipAddress + '-RTP.' + this.work.tuner);
    this.setupService.setCurrentForm(this.currentForm);
    this.topElement.nativeElement.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  // good response to add: {"int": 19}
  saveObserver = {
    next: (x: any) => {
      if (this.card.CardId && x.bool)
        this.work.successCount++;
      else if (!this.card.CardId && x.int) {
        this.work.successCount++;
        this.card.CardId = x.int;
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
          this.captureCardService.UpdateCaptureCard(card.CardId, 'signal_timeout', String(this.card.SignalTimeout))
            .subscribe(this.saveObserver);
          this.captureCardService.UpdateCaptureCard(card.CardId, 'channel_timeout', String(this.card.ChannelTimeout))
            .subscribe(this.saveObserver);
        }
      });
    }
    else {
      this.captureCardService.AddCaptureCard(this.card).subscribe(this.saveObserver);
    }
  }

}
