import { HttpClientModule } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { ConfigService } from './config.service';

describe('ConfigServiceService', () => {
  let service: ConfigService;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [ HttpClientModule ]
    });
    service = TestBed.inject(ConfigService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
