import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';
import { Card } from 'primeng/card';
import { ChannelEditorComponent } from '../../settings/channel-editor/channel-editor.component';
import { Button } from 'primeng/button';
import { TranslatePipe } from '@ngx-translate/core';

@Component({
    selector: 'app-wiz-channeledit',
    templateUrl: './wiz-channeledit.component.html',
    styleUrls: ['./wiz-channeledit.component.css'],
    standalone: true,
    imports: [Card, ChannelEditorComponent, Button, TranslatePipe]
})
export class WizChanneleditComponent implements OnInit {

  constructor(public router: Router) { }

  ngOnInit(): void {
  }

}
