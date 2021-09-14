import { Component, OnInit } from '@angular/core';
import { MythService } from '../services/myth.service';

@Component({
  selector: 'app-home',
  templateUrl: './home.component.html',
  styleUrls: ['./home.component.css']
})
export class HomeComponent implements OnInit {
  hostname = '';

  constructor(private mythService: MythService) { }

  ngOnInit(): void {
    this.mythService.GetHostName().subscribe(data => {
      console.log(data);
      this.hostname = data.HostName;
    });
  }

}
