import { AfterViewInit, Component, Input, OnInit, ViewChild } from '@angular/core';
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
  @ViewChild("cetonform")
  currentForm!: NgForm;

  work = {
    ipAddress : '',
    tuner: '',
    successCount: 0,
    errorCount:   0,
  };

  constructor(private captureCardService : CaptureCardService, private setupService: SetupService) {
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
  }

  showHelp() {
    console.log("show help clicked");
    console.log(this);
  }

  saveObserver = {
    next: (x: any) => {
        if (x.bool)
            this.work.successCount++;
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

}
