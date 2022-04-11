import { Component, OnInit, Input } from '@angular/core';
import { Encoder, TVState } from 'src/app/services/interfaces/encoder.interface';

@Component({
  selector: 'app-status-encoders',
  templateUrl: './encoders.component.html',
  styleUrls: ['./encoders.component.css', '../../status.component.css']
})
export class EncodersComponent implements OnInit {
  @Input() encoders? : Encoder[];

  constructor() { }

  ngOnInit(): void {
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
