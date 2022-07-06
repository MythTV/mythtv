import { ComponentFixture, TestBed } from '@angular/core/testing';

import { DvbComponent } from './dvb.component';

describe('DvbComponent', () => {
  let component: DvbComponent;
  let fixture: ComponentFixture<DvbComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ DvbComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(DvbComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
