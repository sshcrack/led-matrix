import * as React from 'react';
import { useState, useEffect } from 'react';
import { View, Image } from 'react-native';
import Toast from 'react-native-toast-message';
import { PluginInfo, InstalledPlugin, InstallationStatus } from '~/components/apiTypes/marketplace';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import { Button } from '~/components/ui/button';
import { Card, CardContent, CardHeader } from '~/components/ui/card';
import { Badge } from '~/components/ui/badge';
import { Text } from '~/components/ui/text';
import { Activity } from '~/lib/icons/Activity';
import { Download } from '~/lib/icons/Download';
import { Check } from '~/lib/icons/Check';
import { AlertCircle } from '~/lib/icons/AlertCircle';
import { Loader2 } from '~/lib/icons/Loader2';

interface PluginCardProps {
  plugin: PluginInfo;
  installedPlugins: InstalledPlugin[];
  onInstallationChange: () => void;
}

export default function PluginCard({ plugin, installedPlugins, onInstallationChange }: PluginCardProps) {
  const [status, setStatus] = useState<InstallationStatus>('not_installed');
  const [isOperating, setIsOperating] = useState(false);
  const apiUrl = useApiUrl();

  const installedPlugin = installedPlugins.find(p => p.id === plugin.id);

  useEffect(() => {
    if (installedPlugin) {
      if (installedPlugin.version !== plugin.version) {
        setStatus('update_available');
      } else {
        setStatus('installed');
      }
    } else {
      setStatus('not_installed');
    }
  }, [installedPlugin, plugin.version]);

  const handleInstall = async () => {
    setIsOperating(true);
    try {
      const response = await fetch(apiUrl + '/marketplace/install', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          plugin_id: plugin.id,
          version: plugin.version,
        }),
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      setStatus('installing');
      Toast.show({
        type: 'success',
        text1: 'Installation started',
        text2: `Installing ${plugin.name}...`,
      });

      // Poll for status updates
      const pollStatus = setInterval(async () => {
        try {
          const statusResponse = await fetch(apiUrl + `/marketplace/status/${plugin.id}`);
          if (statusResponse.ok) {
            const statusData = await statusResponse.json();
            setStatus(statusData.status_string);
            
            if (statusData.status_string === 'installed') {
              clearInterval(pollStatus);
              onInstallationChange();
              Toast.show({
                type: 'success',
                text1: 'Installation complete',
                text2: `${plugin.name} has been installed successfully`,
              });
            } else if (statusData.status_string === 'error') {
              clearInterval(pollStatus);
              Toast.show({
                type: 'error',
                text1: 'Installation failed',
                text2: statusData.error_message || 'Unknown error occurred',
              });
            }
          }
        } catch (e) {
          // Ignore polling errors
        }
      }, 2000);

      // Stop polling after 5 minutes
      setTimeout(() => {
        clearInterval(pollStatus);
      }, 300000);

    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Installation failed',
        text2: error.message || 'Failed to start installation',
      });
      setStatus('error');
    } finally {
      setIsOperating(false);
    }
  };

  const handleUninstall = async () => {
    setIsOperating(true);
    try {
      const response = await fetch(apiUrl + '/marketplace/uninstall', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          plugin_id: plugin.id,
        }),
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      setStatus('not_installed');
      onInstallationChange();
      Toast.show({
        type: 'success',
        text1: 'Uninstallation complete',
        text2: `${plugin.name} has been removed`,
      });

    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Uninstallation failed',
        text2: error.message || 'Failed to uninstall plugin',
      });
    } finally {
      setIsOperating(false);
    }
  };

  const getStatusButton = () => {
    const isDisabled = isOperating || status === 'downloading' || status === 'installing';
    
    switch (status) {
      case 'not_installed':
        return (
          <Button onPress={handleInstall} disabled={isDisabled} className="w-full">
            <View className="flex flex-row items-center gap-2">
              <Download width={16} height={16} />
              <Text>Install</Text>
            </View>
          </Button>
        );
      
      case 'installed':
        return (
          <Button onPress={handleUninstall} variant="destructive" disabled={isDisabled} className="w-full">
            <View className="flex flex-row items-center gap-2">
              <Check width={16} height={16} />
              <Text>Uninstall</Text>
            </View>
          </Button>
        );
      
      case 'update_available':
        return (
          <Button onPress={handleInstall} disabled={isDisabled} className="w-full">
            <View className="flex flex-row items-center gap-2">
              <Download width={16} height={16} />
              <Text>Update</Text>
            </View>
          </Button>
        );
      
      case 'downloading':
      case 'installing':
        return (
          <Button disabled className="w-full">
            <View className="flex flex-row items-center gap-2">
              <Loader2 width={16} height={16} className="animate-spin" />
              <Text>{status === 'downloading' ? 'Downloading...' : 'Installing...'}</Text>
            </View>
          </Button>
        );
      
      case 'error':
        return (
          <Button onPress={handleInstall} variant="outline" disabled={isDisabled} className="w-full">
            <View className="flex flex-row items-center gap-2">
              <AlertCircle width={16} height={16} />
              <Text>Retry</Text>
            </View>
          </Button>
        );
      
      default:
        return null;
    }
  };

  const getStatusBadge = () => {
    switch (status) {
      case 'installed':
        return <Badge variant="success" className="ml-2">Installed</Badge>;
      case 'update_available':
        return <Badge variant="warning" className="ml-2">Update Available</Badge>;
      case 'downloading':
        return <Badge variant="info" className="ml-2">Downloading</Badge>;
      case 'installing':
        return <Badge variant="info" className="ml-2">Installing</Badge>;
      case 'error':
        return <Badge variant="destructive" className="ml-2">Error</Badge>;
      default:
        return null;
    }
  };

  return (
    <Card className="w-full max-w-sm shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-3">
        <View className="flex flex-row items-start justify-between">
          <View className="flex-1">
            <View className="flex flex-row items-center">
              <Text className="text-lg font-bold">{plugin.name}</Text>
              {getStatusBadge()}
            </View>
            <Text className="text-sm text-muted-foreground mt-1">
              by {plugin.author} • v{plugin.version}
            </Text>
          </View>
        </View>
        
        {plugin.image && (
          <View className="mt-3">
            <Image
              source={{ uri: plugin.image }}
              className="w-full h-32 rounded-lg bg-muted"
              resizeMode="cover"
            />
          </View>
        )}
      </CardHeader>

      <CardContent className="pt-0">
        <Text className="text-sm text-muted-foreground mb-4 leading-5">
          {plugin.description}
        </Text>

        {/* Tags */}
        <View className="flex flex-row flex-wrap gap-1 mb-4">
          {plugin.tags.slice(0, 3).map(tag => (
            <Badge key={tag} variant="outline" className="text-xs">
              {tag}
            </Badge>
          ))}
          {plugin.tags.length > 3 && (
            <Badge variant="outline" className="text-xs">
              +{plugin.tags.length - 3} more
            </Badge>
          )}
        </View>

        {/* Scenes */}
        {plugin.scenes.length > 0 && (
          <View className="mb-4">
            <Text className="text-sm font-medium mb-2">Includes {plugin.scenes.length} scenes:</Text>
            <View className="space-y-1">
              {plugin.scenes.slice(0, 2).map(scene => (
                <Text key={scene.name} className="text-xs text-muted-foreground">
                  • {scene.name}
                </Text>
              ))}
              {plugin.scenes.length > 2 && (
                <Text className="text-xs text-muted-foreground">
                  • And {plugin.scenes.length - 2} more...
                </Text>
              )}
            </View>
          </View>
        )}

        {getStatusButton()}
      </CardContent>
    </Card>
  );
}