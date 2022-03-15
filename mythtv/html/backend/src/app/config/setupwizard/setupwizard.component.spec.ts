import { HttpClientModule } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { RouterTestingModule } from '@angular/router/testing';
import { TranslateModule } from '@ngx-translate/core';

import { SetupWizardComponent } from './setupwizard.component';

describe('SetupWizardComponent', () => {
  let component: SetupWizardComponent;
  let fixture: ComponentFixture<SetupWizardComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SetupWizardComponent ],
      imports:      [ HttpClientModule,
                      RouterTestingModule,
                      TranslateModule.forRoot() ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(SetupWizardComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
