import { HttpClientModule } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { GuideService } from './guide.service';

describe('GuideService', () => {
  let service: GuideService;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [ HttpClientModule ]
    });
    service = TestBed.inject(GuideService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
