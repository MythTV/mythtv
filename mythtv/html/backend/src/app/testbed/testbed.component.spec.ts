import { ComponentFixture, TestBed } from '@angular/core/testing';

import { TestbedComponent } from './testbed.component';

describe('TestbedComponent', () => {
  let component: TestbedComponent;
  let fixture: ComponentFixture<TestbedComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ TestbedComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(TestbedComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
