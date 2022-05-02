import { ComponentFixture, TestBed } from '@angular/core/testing';

import { BackendControlComponent } from './backend-control.component';

describe('BackendControlComponent', () => {
  let component: BackendControlComponent;
  let fixture: ComponentFixture<BackendControlComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ BackendControlComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(BackendControlComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
