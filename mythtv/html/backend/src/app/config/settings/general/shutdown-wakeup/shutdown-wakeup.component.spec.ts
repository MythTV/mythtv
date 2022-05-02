import { ComponentFixture, TestBed } from '@angular/core/testing';

import { ShutdownWakeupComponent } from './shutdown-wakeup.component';

describe('ShutdownWakupComponent', () => {
  let component: ShutdownWakeupComponent;
  let fixture: ComponentFixture<ShutdownWakeupComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ ShutdownWakeupComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(ShutdownWakeupComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
