import { NgForm } from "@angular/forms";
import { PartialObserver } from "rxjs";

export interface DiseqcSettingBase {
    currentForm: NgForm;
    saveForm(parent: number, observer: PartialObserver<any>): void;
}
