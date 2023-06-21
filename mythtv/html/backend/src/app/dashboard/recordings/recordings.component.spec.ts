import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RecordingsComponent } from './recordings.component';

describe('RecordingsComponent', () => {
  let component: RecordingsComponent;
  let fixture: ComponentFixture<RecordingsComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ RecordingsComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(RecordingsComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
