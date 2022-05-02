import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';
import { SetupWizardComponent } from './config/setupwizard/setupwizard.component';
import { DashboardComponent } from './dashboard/dashboard.component';
import { GuideComponent } from './guide/guide.component';
import { StatusComponent } from './status/status.component';
import { TestbedComponent } from './testbed/testbed.component';
import { SettingsComponent } from './config/settings/settings.component';

const routes: Routes = [
  { path: '', component: DashboardComponent },
  { path: 'status', component: StatusComponent },
  { path: 'setupwizard', component: SetupWizardComponent },
  { path: 'settings', component: SettingsComponent },
  { path: 'testbed', component: TestbedComponent },
  { path: 'guide', component: GuideComponent },
];

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule]
})
export class AppRoutingModule { }
