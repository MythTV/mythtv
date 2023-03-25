import { ComponentFixture, TestBed } from '@angular/core/testing';

import { BackendWarningComponent } from './backend-warning.component';

describe('BackendWarningComponent', () => {
  let component: BackendWarningComponent;
  let fixture: ComponentFixture<BackendWarningComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ BackendWarningComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(BackendWarningComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
