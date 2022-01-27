import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';

@Component({
  selector: 'app-restart',
  templateUrl: './restart.component.html',
  styleUrls: ['./restart.component.css']
})
export class RestartComponent implements OnInit {

  constructor(private router: Router) { }

  ngOnInit(): void {
  }

  previousPage() {
    this.router.navigate(['settings/sgsetup']);

    return;
}


  restartBackend() {
    console.log("Restart backend clicked")
  }
}
