import { ComponentFixture, TestBed } from '@angular/core/testing';

import { WizChanneleditComponent } from './wiz-channeledit.component';

describe('WizChanneleditComponent', () => {
  let component: WizChanneleditComponent;
  let fixture: ComponentFixture<WizChanneleditComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ WizChanneleditComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(WizChanneleditComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
