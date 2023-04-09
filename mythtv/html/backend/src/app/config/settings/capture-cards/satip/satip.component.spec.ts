import { ComponentFixture, TestBed } from '@angular/core/testing';

import { SatipComponent } from './satip.component';

describe('SatipComponent', () => {
  let component: SatipComponent;
  let fixture: ComponentFixture<SatipComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ SatipComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(SatipComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
