import { HttpClientModule } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';

import { SetupWizardService } from './setupwizard.service';

describe('SetupwizardService', () => {
  let service: SetupWizardService;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports:  [ HttpClientModule ]
    });
    service = TestBed.inject(SetupWizardService);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
