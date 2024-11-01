import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RecPrioritiesComponent } from './rec-priorities.component';

describe('RecPrioritiesComponent', () => {
  let component: RecPrioritiesComponent;
  let fixture: ComponentFixture<RecPrioritiesComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [RecPrioritiesComponent]
    });
    fixture = TestBed.createComponent(RecPrioritiesComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
