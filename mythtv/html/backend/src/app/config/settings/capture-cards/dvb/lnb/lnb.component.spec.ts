import { ComponentFixture, TestBed } from '@angular/core/testing';

import { LnbComponent } from './lnb.component';

describe('DiseqcComponent', () => {
  let component: LnbComponent;
  let fixture: ComponentFixture<LnbComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ LnbComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(LnbComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
