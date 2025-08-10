import React from 'react';
import { View } from 'react-native';
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Settings } from '~/lib/icons/Settings';
import AddPresetButton from '~/components/home/AddPresetButton';
import Preset from '~/components/home/Preset';
import { ListPresets } from '~/components/apiTypes/list_presets';
import { Status } from '~/components/apiTypes/status';

interface PresetsSectionProps {
  status: { data: Status | null; setRetry: (v: number) => void };
  presets: { data: ListPresets | null; setRetry: (v: number) => void };
  isWeb: boolean;
}

const PresetsSection: React.FC<PresetsSectionProps> = ({ status, presets, isWeb }) => {
  if (!status.data || !presets.data) return null;

  const presetEntries = Object.entries(presets.data);
  const activePreset = status.data?.current;

  return (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-4">
        <View className='w-full flex flex-row items-center justify-between'>
          <View className="flex flex-row items-center gap-3">
            <View className="p-2 bg-success/10 rounded-full">
              <Settings className="text-success" width={20} height={20} />
            </View>
            <Text className="text-xl font-bold">Presets</Text>
          </View>
          <View className='flex flex-col items-end'>
            <Text className="text-sm text-muted-foreground">
              {presetEntries.length} available
            </Text>
          </View>
        </View>
        <CardDescription className="text-base pt-10">
          Select and manage your LED matrix presets
        </CardDescription>
      </CardHeader>
      <CardContent className="pt-0">
        <View className={`flex flex-row flex-wrap gap-4 ${isWeb ? 'justify-start' : 'justify-center'}`}>
          {presetEntries.map(([key, preset]) => (
            <Preset
              key={key}
              preset={preset}
              name={key}
              isActive={activePreset === key}
              setStatusRefresh={() => status.setRetry(Math.random())}
              setPresetRefresh={() => presets.setRetry(Math.random())}
            />
          ))}
          <AddPresetButton
            presetNames={Object.keys(presets.data)}
            setRetry={() => presets.setRetry(Math.random())}
          />
        </View>
      </CardContent>
    </Card>
  );
};

export default PresetsSection;
