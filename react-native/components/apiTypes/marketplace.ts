export interface SceneInfo {
  name: string;
  description: string;
}

export interface BinaryInfo {
  url: string;
  sha512: string;
  size: number;
}

export interface ReleaseInfo {
  matrix?: BinaryInfo;
  desktop?: BinaryInfo;
}

export interface CompatibilityInfo {
  matrix_version?: string;
  desktop_version?: string;
}

export interface PluginInfo {
  id: string;
  name: string;
  description: string;
  version: string;
  author: string;
  tags: string[];
  image?: string;
  scenes: SceneInfo[];
  releases: Record<string, ReleaseInfo>;
  compatibility?: CompatibilityInfo;
  dependencies: string[];
}

export interface MarketplaceIndex {
  version: string;
  plugins: PluginInfo[];
}

export type InstallationStatus = 
  | 'not_installed' 
  | 'installed' 
  | 'update_available' 
  | 'downloading' 
  | 'installing' 
  | 'error';

export interface InstalledPlugin {
  id: string;
  version: string;
  install_path: string;
  enabled: boolean;
}

export interface PluginStatusResponse {
  plugin_id: string;
  status: number;
  status_string: InstallationStatus;
}

export interface InstallationProgress {
  plugin_id: string;
  status: InstallationStatus;
  progress: number; // 0.0 to 1.0
  error_message?: string;
}