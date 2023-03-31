import { ComponentFixture, TestBed } from '@angular/core/testing';

import { V4l2Component } from './v4l2.component';

describe('V4l2Component', () => {
  let component: V4l2Component;
  let fixture: ComponentFixture<V4l2Component>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ V4l2Component ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(V4l2Component);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
