import { Injectable, isDevMode } from '@angular/core';
import { Observable } from 'rxjs';
import { webSocket, WebSocketSubject } from 'rxjs/webSocket';

@Injectable({
  providedIn: 'root'
})
export class WebsocketService {
  private socket$! : WebSocketSubject<any>;
  ws_url = '';

  constructor() {
    if (isDevMode()) {
      this.ws_url = "ws://localhost:6544";
    } else {
      this.ws_url = "ws://" + document.location.host;
    }
    this.connect();
  }

  public connect() : void {
    if (!this.socket$ || this.socket$.closed) {
      console.log("websocket url: " + this.ws_url)
      this.socket$ = webSocket({
        url: this.ws_url,
        binaryType: 'arraybuffer',
        serializer: v => v as ArrayBuffer,
        deserializer: v => v.data,
        openObserver: {
          next: () => {
              console.log('websocket connection opened');
          },
        },
        closeObserver: {
          next: () => {
            console.log('websocket connection closed, reconnecting');
            this.connect();
          }
        },
      });
    }
  }

  public sendMessage(msg: string) {
    this.socket$.next(msg);
  }

  public getData() : Observable<any> {
    return this.socket$.asObservable();
  }

  public enableMessages() {
    this.sendMessage("WS_EVENT_ENABLE");
  }

  public setEventFilter(filter: string) {
    this.sendMessage("WS_EVENT_SET_FILTER " + filter);
  }
}
