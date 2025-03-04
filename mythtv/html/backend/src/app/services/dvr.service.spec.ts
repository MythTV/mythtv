import { provideHttpClient, withInterceptorsFromDi } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { DvrService } from './dvr.service';

describe('DvrService', () => {
  let service: DvrService;

  beforeEach(() => {
    TestBed.configureTestingModule({
    imports: [],
    providers: [provideHttpClient(withInterceptorsFromDi())]
});
    service = TestBed.inject(DvrService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
