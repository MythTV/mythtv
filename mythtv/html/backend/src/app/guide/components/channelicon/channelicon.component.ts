import { Component, OnInit, Input } from '@angular/core';
import { Channel } from 'src/app/services/interfaces/channel.interface';

@Component({
  selector: 'app-guide-channelicon',
  templateUrl: './channelicon.component.html',
  styleUrls: ['./channelicon.component.css']
})
export class ChannelIconComponent implements OnInit {
  @Input() channel!: Channel;

  constructor() { }

  ngOnInit(): void {
  }

  IconUrl(url: string) : string {
    if (url == null || url.length == 0) {
      return "/images/tv/channel_icon.svg";
    } else {
      return url;
    }
  }
}
