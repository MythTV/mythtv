import { ComponentFixture, TestBed } from '@angular/core/testing';

import { EncodersComponent } from './encoders.component';

describe('EncodersComponent', () => {
  let component: EncodersComponent;
  let fixture: ComponentFixture<EncodersComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ EncodersComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(EncodersComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
