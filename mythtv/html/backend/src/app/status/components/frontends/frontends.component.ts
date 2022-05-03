import { Component, OnInit, Input } from '@angular/core';
import { Frontend } from "src/app/services/interfaces/frontend.interface";

@Component({
  selector: 'app-status-frontends',
  templateUrl: './frontends.component.html',
  styleUrls: ['./frontends.component.css', '../../status.component.css']
})
export class FrontendsComponent implements OnInit {
  @Input() frontends? : Frontend[];

  constructor() { }

  ngOnInit(): void {
  }

}
