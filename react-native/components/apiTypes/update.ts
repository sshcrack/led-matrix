export interface UpdateStatus {
    auto_update_enabled: boolean;
    check_interval_hours: number;
    current_version: string;
    latest_version: string;
    update_available: boolean;
    status: number; // UpdateStatus enum: 0=IDLE, 1=CHECKING, 2=DOWNLOADING, 3=INSTALLING, 4=ERROR, 5=SUCCESS
    error_message: string;
}

export interface UpdateInfo {
    update_available: boolean;
    version?: string;
    download_url?: string;
    body?: string;
    is_prerelease?: boolean;
    message?: string;
}

export interface UpdateConfig {
    auto_update_enabled: boolean;
    check_interval_hours: number;
}

export interface UpdateInstallResponse {
    message: string;
    status: string;
}

export interface Release {
    version: string;
    name: string;
    body: string;
    published_at: string;
    is_prerelease: boolean;
    is_draft: boolean;
    download_url?: string;
    download_size?: number;
}

export const UpdateStatusNames = {
    0: 'Idle',
    1: 'Checking for updates',
    2: 'Downloading update',
    3: 'Installing update',
    4: 'Error',
    5: 'Success'
} as const;