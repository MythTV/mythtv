import { NgModule } from '@angular/core';
import { BrowserModule } from '@angular/platform-browser';
import { HttpClientModule } from '@angular/common/http';

import { AppRoutingModule } from './app-routing.module';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';

import { MatIconModule } from '@angular/material/icon';
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatSidenavModule } from '@angular/material/sidenav';
import { MatListModule } from '@angular/material/list';
import { MatTooltipModule } from '@angular/material/tooltip';

import { AppComponent } from './app.component';
import { NavbarComponent } from './layout/navbar/navbar.component';
import { SidenavComponent } from './layout/sidenav/sidenav.component';
import { SettingsComponent } from './config/settings/settings.component';
import { HomeComponent } from './home/home.component';
import { StatusComponent } from './status/status.component';
import { EncodersComponent } from './status/components/encoders/encoders.component';
import { BackendsComponent } from './status/components/backends/backends.component';
import { FrontendsComponent } from './status/components/frontends/frontends.component';
import { ScheduledComponent } from './status/components/scheduled/scheduled.component';
import { JobqueueComponent } from './status/components/jobqueue/jobqueue.component';
import { MachineinfoComponent } from './status/components/machineinfo/machineinfo.component';

@NgModule({
  declarations: [
    AppComponent,
    NavbarComponent,
    SidenavComponent,
    SettingsComponent,
    HomeComponent,
    StatusComponent,
    EncodersComponent,
    BackendsComponent,
    FrontendsComponent,
    ScheduledComponent,
    JobqueueComponent,
    MachineinfoComponent
  ],
  imports: [
    BrowserModule,
    HttpClientModule,
    AppRoutingModule,
    BrowserAnimationsModule,
    MatIconModule,
    MatListModule,
    MatToolbarModule,
    MatTooltipModule,
    MatSidenavModule
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
