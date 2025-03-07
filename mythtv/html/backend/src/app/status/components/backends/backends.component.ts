import { Component, OnInit, Input } from '@angular/core';
import { Backend } from 'src/app/services/interfaces/backend.interface';
import { NgIf, NgFor } from '@angular/common';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-status-backends',
    templateUrl: './backends.component.html',
    styleUrls: ['./backends.component.css', '../../status.component.css'],
    standalone: true,
    imports: [NgIf, NgFor, TranslatePipe]
})
export class BackendsComponent implements OnInit {
  @Input() backends? : Backend[];

  constructor() { }

  ngOnInit(): void {
  }

}
