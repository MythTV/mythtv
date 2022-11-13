import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';
import { SetupWizardComponent } from './config/setupwizard/setupwizard.component';
import { DashboardComponent } from './dashboard/dashboard.component';
import { GuideComponent } from './guide/guide.component';
import { StatusComponent } from './status/status.component';
import { TestbedComponent } from './testbed/testbed.component';
import { SettingsComponent } from './config/settings/general/general-settings.component';
import { CanDeactivateGuardService } from './can-deactivate-guard.service';
import { CaptureCardsComponent } from './config/settings/capture-cards/capture-cards.component';
import { VideoSourcesComponent } from './config/settings/video-sources/video-sources.component';
import { InputConnectionsComponent } from './config/settings/input-connections/input-connections.component';
import { StorageGroupsComponent } from './config/settings/storage-groups/storage-groups.component';

const routes: Routes = [
  { path: '', component: DashboardComponent },
  { path: 'status', component: StatusComponent },
  { path: 'setupwizard', component: SetupWizardComponent },
  {
    path: 'settings/general', component: SettingsComponent,
    canDeactivate: [CanDeactivateGuardService]
  },
  {
    path: 'settings/capture-cards', component: CaptureCardsComponent,
    canDeactivate: [CanDeactivateGuardService]
  },
  {
    path: 'settings/video-sources', component: VideoSourcesComponent,
    canDeactivate: [CanDeactivateGuardService]
  },
  {
    path: 'settings/input-connections', component: InputConnectionsComponent,
    canDeactivate: [CanDeactivateGuardService]
  },
  {
    path: 'settings/storage-groups', component: StorageGroupsComponent,
    canDeactivate: [CanDeactivateGuardService]
  },
  { path: 'testbed', component: TestbedComponent },
  { path: 'guide', component: GuideComponent },
];

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule],
  providers: [CanDeactivateGuardService]
})
export class AppRoutingModule { }
