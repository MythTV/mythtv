import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RotorComponent } from './rotor.component';

describe('RotorComponent', () => {
  let component: RotorComponent;
  let fixture: ComponentFixture<RotorComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ RotorComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(RotorComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
