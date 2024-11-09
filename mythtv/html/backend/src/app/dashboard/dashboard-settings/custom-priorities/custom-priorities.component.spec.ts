import { ComponentFixture, TestBed } from '@angular/core/testing';

import { CustomPrioritiesComponent } from './custom-priorities.component';

describe('CustomPrioritiesComponent', () => {
  let component: CustomPrioritiesComponent;
  let fixture: ComponentFixture<CustomPrioritiesComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [CustomPrioritiesComponent]
    });
    fixture = TestBed.createComponent(CustomPrioritiesComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
