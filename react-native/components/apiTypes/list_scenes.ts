export interface ListScenes {
    name:       string;
    properties: Property[];
}

export interface Property {
    default_value: boolean | number;
    name:          string;
}
