import { Component, OnInit, Input } from '@angular/core';
import { Channel } from 'src/app/services/interfaces/channel.interface';
import { GuideComponent } from '../../guide.component';
import { NgIf } from '@angular/common';

@Component({
    selector: 'app-guide-channelicon',
    templateUrl: './channelicon.component.html',
    styleUrls: ['./channelicon.component.css'],
    standalone: true,
    imports: [NgIf]
})
export class ChannelIconComponent implements OnInit {
  @Input() channel!: Channel;
  @Input() guideComponent!: GuideComponent;

  constructor() { }

  ngOnInit(): void {
  }

}
