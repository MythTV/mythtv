import { Component, OnInit } from '@angular/core';
import { Router } from '@angular/router';
import { WizardData } from 'src/app/services/interfaces/wizarddata.interface';
import { SetupWizardService } from 'src/app/services/setupwizard.service';

@Component({
  selector: 'app-restart',
  templateUrl: './restart.component.html',
  styleUrls: ['./restart.component.css']
})
export class RestartComponent implements OnInit {

  m_wizardData!: WizardData;

  constructor(private router: Router,
              private wizardService: SetupWizardService) { }

  ngOnInit(): void {
    this.m_wizardData = this.wizardService.getWizardData();
  }

  previousPage() {
    this.router.navigate(['settings/sgsetup']);

    return;
}


  restartBackend() {
    console.log("Restart backend clicked")
  }
}
