import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';
import { SetupWizardComponent } from './config/setupwizard/setupwizard.component';
import { DashboardComponent } from './dashboard/dashboard.component';
import { CanDeactivateGuardService } from './can-deactivate-guard.service';

const routes: Routes = [
  { path: 'dashboard', component: DashboardComponent },
  { path: 'setupwizard', component: SetupWizardComponent },
];

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule],
  providers: [CanDeactivateGuardService]
})
export class AppRoutingModule { }
