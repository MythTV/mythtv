import { ComponentFixture, TestBed } from '@angular/core/testing';

import { IconnectionComponent } from './iconnection.component';

describe('IconnectionComponent', () => {
  let component: IconnectionComponent;
  let fixture: ComponentFixture<IconnectionComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ IconnectionComponent ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(IconnectionComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
