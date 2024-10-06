import { Component, OnInit, Input } from '@angular/core';
import { CaptureCardService } from 'src/app/services/capture-card.service';
import { CaptureCardList, CardAndInput } from 'src/app/services/interfaces/capture-card.interface';
import { Encoder, TVState } from 'src/app/services/interfaces/encoder.interface';
import { UtilityService } from 'src/app/services/utility.service';

@Component({
  selector: 'app-status-encoders',
  templateUrl: './encoders.component.html',
  styleUrls: ['./encoders.component.css', '../../status.component.css']
})
export class EncodersComponent implements OnInit {
  @Input() encoders? : Encoder[];
  m_Cards: CardAndInput[] = [];
  
  constructor(public utility: UtilityService,
    private captureCardService: CaptureCardService) { }

  ngOnInit(): void {
    this.captureCardService.GetCaptureCardList('', '').subscribe(data => {
      this.m_Cards = data.CaptureCardList.CaptureCards;
    })
  }

  cardDetails(id: number) {
    let card = this.m_Cards.find((el) => el.CardId == id );
    if (card)
      return card.CardType + ': ' + card.VideoDevice;
    return ' ';
  }

  EncoderStatusText(state: number) : string {
    switch(state) {
      case TVState.kState_Error:
        return "Error";
      case TVState.kState_None:
        return "Idle";
      case TVState.kState_WatchingLiveTV:
        return "Watching Live TV";
      case TVState.kState_WatchingPreRecorded:
        return "Watching Pre Recorded";
      case TVState.kState_WatchingVideo:
        return "Watching Video";
      case TVState.kState_WatchingDVD:
        return "Watching DVD";
      case TVState.kState_WatchingBD:
        return "Watching BD";
      case TVState.kState_WatchingRecording:
        return "Watching Recording";
      case TVState.kState_RecordingOnly:
        return "Recording";
      case TVState.kState_ChangingState:
        return "Changing State";
      default:
        return "unknown";
    }
  }
}
