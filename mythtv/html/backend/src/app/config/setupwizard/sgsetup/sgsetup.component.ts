import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';

@Component({
  selector: 'app-sgsetup',
  templateUrl: './sgsetup.component.html',
  styleUrls: ['./sgsetup.component.css']
})
export class SgsetupComponent implements OnInit {

  constructor(private router: Router,) { }

  ngOnInit(): void {
  }

  previousPage() {
    this.router.navigate(['settings/backendnetwork']);
    return;
}


nextPage() {
    this.router.navigate(['settings/restart']);
    return;
}
}
