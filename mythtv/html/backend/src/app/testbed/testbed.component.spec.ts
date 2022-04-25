import { HttpClientModule } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { TranslateModule } from '@ngx-translate/core';

import { TestbedComponent } from './testbed.component';

describe('TestbedComponent', () => {
  let component: TestbedComponent;
  let fixture: ComponentFixture<TestbedComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ TestbedComponent ],
      imports:      [ HttpClientModule,
                      TranslateModule.forRoot() ]
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
