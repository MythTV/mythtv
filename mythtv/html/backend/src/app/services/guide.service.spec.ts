import { TestBed } from '@angular/core/testing';

import { GuideService } from './guide.service';

describe('GuideService', () => {
  let service: GuideService;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(GuideService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
