import { Component, OnInit, Input } from '@angular/core';
import { Encoder } from 'src/app/services/interfaces/encoder.interface';

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

}
