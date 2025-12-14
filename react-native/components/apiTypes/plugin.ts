export interface PluginRelease {
    version: string;
    tag_name: string;
    download_url: string;
    name: string;
    body: string;
    prerelease: boolean;
    published_at: string;
    size: number;
}

export interface InstallPluginRequest {
    url: string;
    filename: string;
}
