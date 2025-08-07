import { Link } from 'expo-router';
import * as React from 'react';
import { useEffect, useState } from 'react';
import { Dimensions, RefreshControl, ScrollView, View, TextInput } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { MarketplaceIndex, InstalledPlugin } from '~/components/apiTypes/marketplace';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import PluginCard from '~/components/marketplace/PluginCard';
import { Button } from '~/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '~/components/ui/card';
import { Input } from '~/components/ui/input';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';
import { Activity } from '~/lib/icons/Activity';
import { Download } from '~/lib/icons/Download';
import { Search } from '~/lib/icons/Search';
import { ShoppingBag } from '~/lib/icons/ShoppingBag';

export default function MarketplaceScreen() {
  const marketplace = useFetch<MarketplaceIndex>(`/marketplace/index`);
  const installed = useFetch<InstalledPlugin[]>(`/marketplace/installed`);
  
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string>('all');
  const [manualRefresh, setManualRefresh] = useState(false);
  const apiUrl = useApiUrl();

  const refreshMarketplace = async () => {
    try {
      await fetch(apiUrl + `/marketplace/refresh`);
      Toast.show({
        type: 'success',
        text1: 'Marketplace refreshed',
        text2: 'Updated plugin list from remote repository'
      });
    } catch (e: any) {
      Toast.show({
        type: 'error',
        text1: 'Error refreshing marketplace',
        text2: e.message
      });
    }
  };

  const setRetry = () => {
    setManualRefresh(true);
    marketplace.setRetry(Math.random());
    installed.setRetry(Math.random());
  };

  useEffect(() => {
    if (!marketplace.isLoading && !installed.isLoading) {
      setManualRefresh(false);
    }
  }, [marketplace.isLoading, installed.isLoading]);

  const { width } = Dimensions.get('window');
  const isWeb = width > 768;

  // Filter plugins based on search and category
  const filteredPlugins = React.useMemo(() => {
    if (!marketplace.data?.plugins) return [];
    
    return marketplace.data.plugins.filter(plugin => {
      const matchesSearch = !searchQuery || 
        plugin.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        plugin.description.toLowerCase().includes(searchQuery.toLowerCase()) ||
        plugin.tags.some(tag => tag.toLowerCase().includes(searchQuery.toLowerCase()));
        
      const matchesCategory = selectedCategory === 'all' || 
        plugin.tags.includes(selectedCategory);
        
      return matchesSearch && matchesCategory;
    });
  }, [marketplace.data?.plugins, searchQuery, selectedCategory]);

  // Get all unique categories from plugins
  const categories = React.useMemo(() => {
    if (!marketplace.data?.plugins) return ['all'];
    
    const allTags = marketplace.data.plugins.flatMap(plugin => plugin.tags);
    const uniqueTags = Array.from(new Set(allTags));
    return ['all', ...uniqueTags.sort()];
  }, [marketplace.data?.plugins]);

  const isLoading = marketplace.isLoading || installed.isLoading;
  const error = marketplace.error ?? installed.error;

  const HeaderCard = () => (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <CardTitle className="flex flex-row items-center gap-3">
          <View className="p-2 bg-primary/10 rounded-full">
            <ShoppingBag className="text-primary" width={24} height={24} />
          </View>
          <Text className="text-2xl font-bold">Plugin Marketplace</Text>
        </CardTitle>
        <CardDescription className="text-base">
          Discover and install plugins to enhance your LED matrix
        </CardDescription>
      </CardHeader>
      <CardContent className="pt-0">
        <View className="flex flex-row gap-3 mb-4">
          <View className="flex-1">
            <View className="relative">
              <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-muted-foreground" width={16} height={16} />
              <Input
                placeholder="Search plugins..."
                value={searchQuery}
                onChangeText={setSearchQuery}
                className="pl-10"
              />
            </View>
          </View>
          <Button onPress={refreshMarketplace} variant="outline">
            <Download width={16} height={16} />
          </Button>
        </View>
        
        {/* Category Filter */}
        <ScrollView horizontal showsHorizontalScrollIndicator={false} className="mb-4">
          <View className="flex flex-row gap-2">
            {categories.map(category => (
              <Button
                key={category}
                variant={selectedCategory === category ? "default" : "outline"}
                size="sm"
                onPress={() => setSelectedCategory(category)}
                className="mr-2"
              >
                <Text className="capitalize">
                  {category === 'all' ? 'All' : category}
                </Text>
              </Button>
            ))}
          </View>
        </ScrollView>
      </CardContent>
    </Card>
  );

  const PluginsSection = () => {
    if (!marketplace.data || !installed.data) return null;

    return (
      <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
        <CardHeader className="pb-4">
          <View className='w-full flex flex-row items-center justify-between'>
            <View className="flex flex-row items-center gap-3">
              <View className="p-2 bg-success/10 rounded-full">
                <Activity className="text-success" width={20} height={20} />
              </View>
              <Text className="text-xl font-bold">Available Plugins</Text>
            </View>
            <View className='flex flex-col items-end'>
              <Text className="text-sm text-muted-foreground">
                {filteredPlugins.length} plugins
              </Text>
            </View>
          </View>
        </CardHeader>
        <CardContent className="pt-0">
          <View className={`flex flex-row flex-wrap gap-4 ${isWeb ? 'justify-start' : 'justify-center'}`}>
            {filteredPlugins.map(plugin => (
              <PluginCard
                key={plugin.id}
                plugin={plugin}
                installedPlugins={installed.data!}
                onInstallationChange={() => {
                  installed.setRetry(Math.random());
                }}
              />
            ))}
            {filteredPlugins.length === 0 && (
              <View className="w-full py-8 flex items-center justify-center">
                <Text className="text-muted-foreground text-lg">
                  No plugins found matching your criteria
                </Text>
              </View>
            )}
          </View>
        </CardContent>
      </Card>
    );
  };

  const ErrorCard = () => (
    <Card className="w-full shadow-lg border-destructive/20 bg-destructive/5">
      <CardContent className="pt-6">
        <View className="flex items-center gap-4">
          <View className="p-3 bg-destructive/10 rounded-full">
            <Activity className="text-destructive" width={24} height={24} />
          </View>
          <Text className="text-lg font-semibold text-destructive">
            Connection Error
          </Text>
          <Text className="text-center text-muted-foreground">
            {error?.message || "Unable to connect to marketplace"}
          </Text>
          <Button onPress={setRetry} className="mt-2">
            <Text>Retry Connection</Text>
          </Button>
        </View>
      </CardContent>
    </Card>
  );

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
              refreshing={isLoading && manualRefresh}
              onRefresh={setRetry}
              colors={['rgb(99, 102, 241)']}
              tintColor="rgb(99, 102, 241)"
            />
          }
        >
          <View className={`w-full max-w-4xl gap-6 ${isWeb ? 'items-stretch' : 'items-center'}`}>
            {error ? (
              <ErrorCard />
            ) : (
              <>
                <HeaderCard />
                <PluginsSection />
              </>
            )}
          </View>
        </ScrollView>
      </SafeAreaView>
    </SafeAreaProvider>
  );
}