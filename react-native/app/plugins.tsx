import React, { useState } from 'react';
import { RefreshControl, ScrollView, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import useFetch from '~/components/useFetch';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import { PluginRelease } from '~/components/apiTypes/plugin';
import { Card, CardContent, CardHeader, CardTitle } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Button } from '~/components/ui/button';
import { Download } from '~/lib/icons/Download';
import ErrorCard from '~/components/home/ErrorCard';

export default function PluginsScreen() {
    // Default to the main repo for now, assuming plugins might be attached there or user can change it later
    // In a real scenario, this should likely be a dedicated plugins repo
    const releases = useFetch<PluginRelease[]>('/api/plugins/store');
    const apiUrl = useApiUrl();
    const [installing, setInstalling] = useState<string | null>(null);

    const handleInstall = async (release: PluginRelease) => {
        setInstalling(release.tag_name);
        try {
            const response = await fetch(apiUrl + '/api/plugins/install', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    url: release.download_url,
                    filename: release.name || `plugin-${release.tag_name}.zip`
                }),
            });

            if (!response.ok) {
                const text = await response.text();
                throw new Error(text || 'Installation failed');
            }

            Toast.show({
                type: 'success',
                text1: 'Plugin Installed',
                text2: `Successfully installed version ${release.version}`
            });
        } catch (error: any) {
            Toast.show({
                type: 'error',
                text1: 'Installation Failed',
                text2: error.message
            });
        } finally {
            setInstalling(null);
        }
    };

    return (
        <SafeAreaProvider>
            <SafeAreaView className="flex-1 bg-background">
                <ScrollView
                    className="flex-1"
                    contentContainerStyle={{
                        padding: 20,
                        gap: 20,
                        alignItems: 'center'
                    }}
                    refreshControl={
                        <RefreshControl
                            refreshing={releases.isLoading}
                            onRefresh={() => releases.setRetry(Math.random())}
                            colors={['rgb(99, 102, 241)']}
                            tintColor="rgb(99, 102, 241)"
                        />
                    }
                >
                    <View className="w-full max-w-4xl gap-6">
                        {releases.error ? (
                            <ErrorCard error={releases.error} setRetry={() => releases.setRetry(Math.random())} />
                        ) : (
                            Array.isArray(releases.data) ? (
                                releases.data.map((release, index) => (
                                    <Card key={index} className="w-full">
                                        <CardHeader>
                                            <CardTitle className="text-xl flex flex-row justify-between items-center">
                                                <Text>{release.name || release.tag_name}</Text>
                                                <Text className="text-sm font-normal text-muted-foreground">{release.published_at.split('T')[0]}</Text>
                                            </CardTitle>
                                        </CardHeader>
                                        <CardContent className="gap-4">
                                            <Text className="text-foreground">{release.body || "No description provided."}</Text>
                                            <Button
                                                onPress={() => handleInstall(release)}
                                                disabled={installing !== null}
                                            >
                                                <View className="flex flex-row items-center gap-2">
                                                    <Download className="text-primary-foreground" width={20} height={20} color="white" />
                                                    <Text className="text-primary-foreground font-bold">
                                                        {installing === release.tag_name ? 'Installing...' : 'Install'}
                                                    </Text>
                                                </View>
                                            </Button>
                                        </CardContent>
                                    </Card>
                                ))
                            ) : (
                                releases.data ? (
                                    <ErrorCard error={new Error((releases.data as any).error || "Invalid server response")} setRetry={() => releases.setRetry(Math.random())} />
                                ) : null
                            )
                        )}
                        {!releases.isLoading && (!releases.data || releases.data.length === 0) && !releases.error && (
                            <Text className="text-center text-muted-foreground">No plugins found.</Text>
                        )}
                    </View>
                </ScrollView>
            </SafeAreaView>
        </SafeAreaProvider>
    );
}
