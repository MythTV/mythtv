import { ComponentFixture, TestBed } from '@angular/core/testing';

import { BackendWakeupComponent } from './backend-wakeup.component';

describe('BackendWakeupComponent', () => {
  let component: BackendWakeupComponent;
  let fixture: ComponentFixture<BackendWakeupComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ BackendWakeupComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(BackendWakeupComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
