import { Component, OnInit, Input } from '@angular/core';
import { Backend } from 'src/app/services/interfaces/backend.interface';

@Component({
  selector: 'app-status-backends',
  templateUrl: './backends.component.html',
  styleUrls: ['./backends.component.css', '../../status.component.css']
})
export class BackendsComponent implements OnInit {
  @Input() backends? : Backend[];

  constructor() { }

  ngOnInit(): void {
  }

}
