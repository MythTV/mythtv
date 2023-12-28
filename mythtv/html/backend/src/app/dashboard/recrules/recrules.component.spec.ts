import { ComponentFixture, TestBed } from '@angular/core/testing';

import { RecrulesComponent } from './recrules.component';

describe('RecrulesComponent', () => {
  let component: RecrulesComponent;
  let fixture: ComponentFixture<RecrulesComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ RecrulesComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(RecrulesComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
