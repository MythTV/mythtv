import { HttpClientModule } from '@angular/common/http';
import { ComponentFixture, TestBed } from '@angular/core/testing';

import { GuideComponent } from './guide.component';

describe('GuideComponent', () => {
  let component: GuideComponent;
  let fixture: ComponentFixture<GuideComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [ GuideComponent ],
      imports:      [ HttpClientModule ]
    })
    .compileComponents();
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(GuideComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
