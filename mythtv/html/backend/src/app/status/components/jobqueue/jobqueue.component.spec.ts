import { ComponentFixture, TestBed } from '@angular/core/testing';

import { JobqueueComponent } from './jobqueue.component';

describe('JobqueueComponent', () => {
  let component: JobqueueComponent;
  let fixture: ComponentFixture<JobqueueComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ JobqueueComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(JobqueueComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
