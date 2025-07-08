import * as React from 'react';
import { useEffect, useState } from 'react';
import { ScrollView, View, Alert, Platform } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import { Button } from '~/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '~/components/ui/card';
import { Input } from '~/components/ui/input';
import { Label } from '~/components/ui/label';
import { Switch } from '~/components/ui/switch';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';
import { ListPresets } from '~/components/apiTypes/list_presets';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select';

interface Schedule {
  id: string;
  name: string;
  preset_id: string;
  start_hour: number;
  start_minute: number;
  end_hour: number;
  end_minute: number;
  days_of_week: number[];
  enabled: boolean;
}

interface SchedulingStatus {
  enabled: boolean;
  active_preset: string;
}

const DAYS_OF_WEEK = [
  { value: 0, label: 'Sunday' },
  { value: 1, label: 'Monday' },
  { value: 2, label: 'Tuesday' },
  { value: 3, label: 'Wednesday' },
  { value: 4, label: 'Thursday' },
  { value: 5, label: 'Friday' },
  { value: 6, label: 'Saturday' }
];

export default function ScheduleScreen() {
  const apiUrl = useApiUrl();
  const schedules = useFetch<Record<string, Schedule>>('/schedules');
  const schedulingStatus = useFetch<SchedulingStatus>('/scheduling_status');
  const presets = useFetch<ListPresets>('/list_presets');

  const [newSchedule, setNewSchedule] = useState<Partial<Schedule>>({
    name: '',
    preset_id: '',
    start_hour: 9,
    start_minute: 0,
    end_hour: 22,
    end_minute: 0,
    days_of_week: [1, 2, 3, 4, 5], // Monday to Friday
    enabled: true
  });

  const [isCreating, setIsCreating] = useState(false);

  const toggleScheduling = async () => {
    try {
      const newEnabled = !schedulingStatus.data?.enabled;
      await fetch(`${apiUrl}/set_scheduling_enabled?enabled=${newEnabled}`);
      schedulingStatus.setRetry(Math.random());
      Toast.show({
        type: 'success',
        text1: `Scheduling ${newEnabled ? 'enabled' : 'disabled'}`
      });
    } catch (error) {
      Toast.show({
        type: 'error',
        text1: 'Failed to toggle scheduling',
        text2: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };

  const createSchedule = async () => {
    if (!newSchedule.name || !newSchedule.preset_id) {
      Toast.show({
        type: 'error',
        text1: 'Please fill in all required fields'
      });
      return;
    }

    setIsCreating(true);
    try {
      const response = await fetch(`${apiUrl}/schedule`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(newSchedule)
      });

      if (response.ok) {
        setNewSchedule({
          name: '',
          preset_id: '',
          start_hour: 9,
          start_minute: 0,
          end_hour: 22,
          end_minute: 0,
          days_of_week: [1, 2, 3, 4, 5],
          enabled: true
        });
        schedules.setRetry(Math.random());
        Toast.show({
          type: 'success',
          text1: 'Schedule created successfully'
        });
      } else {
        throw new Error('Failed to create schedule');
      }
    } catch (error) {
      Toast.show({
        type: 'error',
        text1: 'Failed to create schedule',
        text2: error instanceof Error ? error.message : 'Unknown error'
      });
    } finally {
      setIsCreating(false);
    }
  };

  const deleteSchedule = async (scheduleId: string) => {
    try {
      const response = await fetch(`${apiUrl}/schedule?id=${scheduleId}`, {
        method: 'DELETE'
      });

      if (response.ok) {
        schedules.setRetry(Math.random());
        Toast.show({
          type: 'success',
          text1: 'Schedule deleted successfully'
        });
      } else {
        throw new Error('Failed to delete schedule');
      }
    } catch (error) {
      Toast.show({
        type: 'error',
        text1: 'Failed to delete schedule',
        text2: error instanceof Error ? error.message : 'Unknown error'
      });
    }
  };

  const formatTime = (hour: number, minute: number) => {
    return `${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}`;
  };

  const formatDays = (days: number[]) => {
    return days.map(day => DAYS_OF_WEEK[day].label).join(', ');
  };

  const toggleDay = (day: number) => {
    const currentDays = newSchedule.days_of_week || [];
    const newDays = currentDays.includes(day)
      ? currentDays.filter(d => d !== day)
      : [...currentDays, day].sort();
    setNewSchedule({ ...newSchedule, days_of_week: newDays });
  };

  return (
    <SafeAreaView className="flex-1 bg-background">
      <ScrollView className="flex-1 p-4">
        {/* Scheduling Status */}
        <Card className="mb-4">
          <CardHeader>
            <CardTitle>Scheduling Control</CardTitle>
            <CardDescription>
              Enable or disable automatic preset scheduling
            </CardDescription>
          </CardHeader>
          <CardContent>
            <View className="flex-row items-center justify-between">
              <Text>Scheduling Enabled</Text>
              <Switch
                checked={schedulingStatus.data?.enabled || false}
                onCheckedChange={toggleScheduling}
              />
            </View>
            {schedulingStatus.data?.active_preset && schedulingStatus.data.active_preset !== 'none' && (
              <Text className="mt-2 text-sm text-muted-foreground">
                Active preset: {schedulingStatus.data.active_preset}
              </Text>
            )}
          </CardContent>
        </Card>

        {/* Create New Schedule */}
        <Card className="mb-4">
          <CardHeader>
            <CardTitle>Create New Schedule</CardTitle>
            <CardDescription>
              Set up automatic preset switching based on time and day
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <View>
              <Label>Schedule Name</Label>
              <Input
                value={newSchedule.name}
                onChangeText={(text) => setNewSchedule({ ...newSchedule, name: text })}
                placeholder="e.g., Work Hours"
              />
            </View>

            <View>
              <Label>Preset</Label>
              <Select
                value={newSchedule.preset_id ? { value: newSchedule.preset_id, label: newSchedule.preset_id } : undefined}
                onValueChange={(option) => setNewSchedule({ ...newSchedule, preset_id: option?.value || '' })}
              >
                <SelectTrigger>
                  <SelectValue placeholder="Select a preset" />
                </SelectTrigger>
                <SelectContent>
                  {Object.keys(presets.data || {}).map((presetId) => (
                    <SelectItem key={presetId} value={presetId} label={presetId}>
                      {presetId}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </View>

            <View className="flex-row space-x-4">
              <View className="flex-1">
                <Label>Start Time</Label>
                <View className="flex-row space-x-2">
                  <Input
                    className="flex-1"
                    value={newSchedule.start_hour?.toString()}
                    onChangeText={(text) => setNewSchedule({ ...newSchedule, start_hour: parseInt(text) || 0 })}
                    placeholder="Hour"
                    keyboardType="numeric"
                  />
                  <Input
                    className="flex-1"
                    value={newSchedule.start_minute?.toString()}
                    onChangeText={(text) => setNewSchedule({ ...newSchedule, start_minute: parseInt(text) || 0 })}
                    placeholder="Minute"
                    keyboardType="numeric"
                  />
                </View>
              </View>

              <View className="flex-1">
                <Label>End Time</Label>
                <View className="flex-row space-x-2">
                  <Input
                    className="flex-1"
                    value={newSchedule.end_hour?.toString()}
                    onChangeText={(text) => setNewSchedule({ ...newSchedule, end_hour: parseInt(text) || 0 })}
                    placeholder="Hour"
                    keyboardType="numeric"
                  />
                  <Input
                    className="flex-1"
                    value={newSchedule.end_minute?.toString()}
                    onChangeText={(text) => setNewSchedule({ ...newSchedule, end_minute: parseInt(text) || 0 })}
                    placeholder="Minute"
                    keyboardType="numeric"
                  />
                </View>
              </View>
            </View>

            <View>
              <Label>Days of Week</Label>
              <View className="flex-row flex-wrap mt-2">
                {DAYS_OF_WEEK.map((day) => (
                  <Button
                    key={day.value}
                    variant={newSchedule.days_of_week?.includes(day.value) ? 'default' : 'outline'}
                    size="sm"
                    className="m-1"
                    onPress={() => toggleDay(day.value)}
                  >
                    <Text>{day.label.slice(0, 3)}</Text>
                  </Button>
                ))}
              </View>
            </View>

            <Button onPress={createSchedule} disabled={isCreating}>
              <Text>{isCreating ? 'Creating...' : 'Create Schedule'}</Text>
            </Button>
          </CardContent>
        </Card>

        {/* Existing Schedules */}
        <Card>
          <CardHeader>
            <CardTitle>Existing Schedules</CardTitle>
            <CardDescription>
              Manage your current schedules
            </CardDescription>
          </CardHeader>
          <CardContent>
            {schedules.data && Object.keys(schedules.data).length > 0 ? (
              Object.entries(schedules.data).map(([id, schedule]) => (
                <Card key={id} className="mb-3">
                  <CardContent className="p-4">
                    <View className="flex-row justify-between items-start mb-2">
                      <Text className="font-medium">{schedule.name}</Text>
                      <View className="flex-row items-center space-x-2">
                        <Switch
                          checked={schedule.enabled}
                          onCheckedChange={() => {
                            // TODO: Implement toggle schedule enabled
                          }}
                        />
                        <Button
                          variant="destructive"
                          size="sm"
                          onPress={() => {
                            if (Platform.OS === 'web') {
                              if (confirm('Are you sure you want to delete this schedule?')) {
                                deleteSchedule(id);
                              }
                            } else {
                              Alert.alert(
                                'Delete Schedule',
                                'Are you sure you want to delete this schedule?',
                                [
                                  { text: 'Cancel', style: 'cancel' },
                                  { text: 'Delete', style: 'destructive', onPress: () => deleteSchedule(id) }
                                ]
                              );
                            }
                          }}
                        >
                          <Text>Delete</Text>
                        </Button>
                      </View>
                    </View>
                    <Text className="text-sm text-muted-foreground">
                      Preset: {schedule.preset_id}
                    </Text>
                    <Text className="text-sm text-muted-foreground">
                      Time: {formatTime(schedule.start_hour, schedule.start_minute)} - {formatTime(schedule.end_hour, schedule.end_minute)}
                    </Text>
                    <Text className="text-sm text-muted-foreground">
                      Days: {formatDays(schedule.days_of_week)}
                    </Text>
                  </CardContent>
                </Card>
              ))
            ) : (
              <Text className="text-center text-muted-foreground py-8">
                No schedules created yet
              </Text>
            )}
          </CardContent>
        </Card>
      </ScrollView>
    </SafeAreaView>
  );
}
