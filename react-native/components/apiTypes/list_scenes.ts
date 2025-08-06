export interface ListScenes {
    name: string;
    properties: Property<any>[];
}

export interface Property<T> {
    default_value: T;
    name: string;
    additional?: any;
    type_id: TypeId
}


export type CollectionProvider = {
    type: "collection";
    uuid: string,
    arguments: {
        images: string[]
    }
}

export type PagesProvider = {
    type: "pages";
    uuid: string,
    arguments: {
        begin: number,
        end: number
    }
}

export type ListProviders = ListScenes
export type ProviderValue = CollectionProvider | PagesProvider | {
    type: string,
    uuid: string,
    arguments: {
        [key: string]: any
    }
};

export type TypeId = "string" | "int" | "double" | "bool" | "float" | "millis" | "json" | "int16_t" | "enum"