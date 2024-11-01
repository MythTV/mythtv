import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RecQualityComponent } from './rec-quality.component';

describe('RecQualityComponent', () => {
  let component: RecQualityComponent;
  let fixture: ComponentFixture<RecQualityComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [RecQualityComponent]
    });
    fixture = TestBed.createComponent(RecQualityComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
