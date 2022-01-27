import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { HttpClientModule } from '@angular/common/http';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';

import { AppRoutingModule } from './app-routing.module';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';

// PrimeNG
import { ButtonModule } from 'primeng/button';
import { SidebarModule } from 'primeng/sidebar';
import { MenubarModule } from 'primeng/menubar';
import { MenuItem } from 'primeng/api';
import { ToolbarModule } from 'primeng/toolbar';
import { CardModule } from 'primeng/card';
import { StepsModule } from 'primeng/steps';
import { TooltipModule } from 'primeng/tooltip';
import { ToastModule } from 'primeng/toast';
import { MessagesModule } from 'primeng/messages';
import { MessageModule } from 'primeng/message';
import { PanelModule } from 'primeng/panel';
import { SkeletonModule } from 'primeng/skeleton';
import { ListboxModule } from 'primeng/listbox';
import { DialogModule } from 'primeng/dialog';
import { DataViewModule } from 'primeng/dataview';
import { ProgressSpinnerModule } from 'primeng/progressspinner';

import { AppComponent } from './app.component';
import { NavbarComponent } from './layout/navbar/navbar.component';
import { SidenavComponent } from './layout/sidenav/sidenav.component';
import { SetupWizardComponent } from './config/setupwizard/setupwizard.component';
import { SetupWizardRoutingModule } from './config/setupwizard/setupwizard-routing.module';
import { DashboardComponent } from './dashboard/dashboard.component';
import { StatusComponent } from './status/status.component';
import { EncodersComponent } from './status/components/encoders/encoders.component';
import { BackendsComponent } from './status/components/backends/backends.component';
import { FrontendsComponent } from './status/components/frontends/frontends.component';
import { ScheduledComponent } from './status/components/scheduled/scheduled.component';
import { JobqueueComponent } from './status/components/jobqueue/jobqueue.component';
import { MachineinfoComponent } from './status/components/machineinfo/machineinfo.component';
import { SelectLanguageComponent } from './config/setupwizard/selectlanguage/selectlanguage.component';
import { DbsetupComponent } from './config/setupwizard/dbsetup/dbsetup.component';
import { BackendnetworkComponent } from './config/setupwizard/backendnetwork/backendnetwork.component';
import { SgsetupComponent } from './config/setupwizard/sgsetup/sgsetup.component';
import { RestartComponent } from './config/setupwizard/restart/restart.component';
import { TestbedComponent } from './testbed/testbed.component';
import { GuideComponent } from './guide/guide.component';
import { ChannelIconComponent } from './guide/components/channelicon/channelicon.component';
import { ProgramEntryComponent } from './guide/components/programentry/programentry.component';
import { TimebarComponent } from './guide/components/timebar/timebar.component';

@NgModule({
  declarations: [
    AppComponent,
    NavbarComponent,
    SidenavComponent,
    SetupWizardComponent,
    DashboardComponent,
    StatusComponent,
    EncodersComponent,
    BackendsComponent,
    FrontendsComponent,
    ScheduledComponent,
    JobqueueComponent,
    MachineinfoComponent,
    SelectLanguageComponent,
    DbsetupComponent,
    BackendnetworkComponent,
    SgsetupComponent,
    RestartComponent,
    TestbedComponent,
    GuideComponent,
    ChannelIconComponent,
    ProgramEntryComponent,
    TimebarComponent,
  ],
  imports: [
    BrowserModule,
    HttpClientModule,
    AppRoutingModule,
    BrowserAnimationsModule,
    FormsModule,
    ReactiveFormsModule,
    SetupWizardRoutingModule,
    // PrimeNG
    SidebarModule,
    ButtonModule,
    MenubarModule,
    ToolbarModule,
    CardModule,
    StepsModule,
    TooltipModule,
    ToastModule,
    MessagesModule,
    MessageModule,
    PanelModule,
    SkeletonModule,
    ListboxModule,
    DialogModule,
    CardModule,
    DataViewModule,
    ProgressSpinnerModule
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
