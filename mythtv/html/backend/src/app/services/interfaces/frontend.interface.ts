export interface Frontend {
    IP: string;
    Name: string;
    OnLine: boolean;
    Port: number;
}

export interface FrontendList {
    Frontends:      Frontend[];
}