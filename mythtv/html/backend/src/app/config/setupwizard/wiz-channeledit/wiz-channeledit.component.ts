import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';

@Component({
  selector: 'app-wiz-channeledit',
  templateUrl: './wiz-channeledit.component.html',
  styleUrls: ['./wiz-channeledit.component.css']
})
export class WizChanneleditComponent implements OnInit {

  constructor(public router: Router) { }

  ngOnInit(): void {
  }

}
