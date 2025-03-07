import { Component, OnInit, Input } from '@angular/core';
import { Frontend } from "src/app/services/interfaces/frontend.interface";
import { NgIf, NgFor } from '@angular/common';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-status-frontends',
    templateUrl: './frontends.component.html',
    styleUrls: ['./frontends.component.css', '../../status.component.css'],
    standalone: true,
    imports: [NgIf, NgFor, TranslatePipe]
})
export class FrontendsComponent implements OnInit {
  @Input() frontends? : Frontend[];

  constructor() { }

  ngOnInit(): void {
  }

}
