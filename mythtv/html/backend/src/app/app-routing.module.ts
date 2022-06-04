import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';
import { SetupWizardComponent } from './config/setupwizard/setupwizard.component';
import { DashboardComponent } from './dashboard/dashboard.component';
import { GuideComponent } from './guide/guide.component';
import { StatusComponent } from './status/status.component';
import { TestbedComponent } from './testbed/testbed.component';
import { SettingsComponent } from './config/settings/general/general-settings.component';
import { CanDeactivateGuardService } from './can-deactivate-guard.service';

const routes: Routes = [
  { path: '', component: DashboardComponent },
  { path: 'status', component: StatusComponent },
  { path: 'setupwizard', component: SetupWizardComponent },
  { path: 'settings/general', component: SettingsComponent,
      canDeactivate: [CanDeactivateGuardService] },
  { path: 'testbed', component: TestbedComponent },
  { path: 'guide', component: GuideComponent },
];

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule],
  providers: [CanDeactivateGuardService]
})
export class AppRoutingModule { }
