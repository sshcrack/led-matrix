import * as React from 'react';
import { useEffect, useState } from 'react';
import { ScrollView, View, Alert, Platform, Dimensions } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider';
import { Button } from '~/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '~/components/ui/card';
import { Input } from '~/components/ui/input';
import { Label } from '~/components/ui/label';
import { StatusIndicator } from '~/components/ui/status-indicator';
import { Switch } from '~/components/ui/switch';
import { Text } from '~/components/ui/text';
import useFetch from '~/components/useFetch';
import { ListPresets } from '~/components/apiTypes/list_presets';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select';
import { Calendar } from '~/lib/icons/Calendar';
import { Clock } from '~/lib/icons/Clock';
import { Trash2 } from '~/lib/icons/Trash2';
import { Plus } from '~/lib/icons/Plus';
import { Settings } from '~/lib/icons/Settings';

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
  { value: 0, label: 'Sun' },
  { value: 1, label: 'Mon' },
  { value: 2, label: 'Tue' },
  { value: 3, label: 'Wed' },
  { value: 4, label: 'Thu' },
  { value: 5, label: 'Fri' },
  { value: 6, label: 'Sat' }
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
    end_hour: 17,
    end_minute: 0,
    days_of_week: [],
    enabled: true
  });

  const [showAddForm, setShowAddForm] = useState(false);
  const [loading, setLoading] = useState(false);

  const { width } = Dimensions.get('window');
  const isWeb = width > 768;

  const toggleScheduling = async (enabled: boolean) => {
    try {
      const response = await fetch(`${apiUrl}/scheduling_status`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled })
      });

      if (!response.ok) throw new Error('Failed to update scheduling status');

      schedulingStatus.setRetry(Math.random());
    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Error',
        text2: error.message || 'Failed to update scheduling status'
      });
    }
  };

  const addSchedule = async () => {
    if (!newSchedule.name || !newSchedule.preset_id || newSchedule.days_of_week?.length === 0) {
      Toast.show({
        type: 'error',
        text1: 'Validation Error',
        text2: 'Please fill in all fields and select at least one day'
      });
      return;
    }

    setLoading(true);
    try {
      const response = await fetch(`${apiUrl}/schedules`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ...newSchedule,
          id: Date.now().toString()
        })
      });

      if (!response.ok) throw new Error('Failed to create schedule');

      setNewSchedule({
        name: '',
        preset_id: '',
        start_hour: 9,
        start_minute: 0,
        end_hour: 17,
        end_minute: 0,
        days_of_week: [],
        enabled: true
      });
      setShowAddForm(false);
      schedules.setRetry(Math.random());

      Toast.show({
        type: 'success',
        text1: 'Success',
        text2: 'Schedule created successfully'
      });
    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Error',
        text2: error.message || 'Failed to create schedule'
      });
    } finally {
      setLoading(false);
    }
  };

  const deleteSchedule = async (id: string) => {
    if (Platform.OS === 'web') {
      if (!confirm('Are you sure you want to delete this schedule?')) return;
    } else {
      Alert.alert(
        'Delete Schedule',
        'Are you sure you want to delete this schedule?',
        [
          { text: 'Cancel', style: 'cancel' },
          { text: 'Delete', style: 'destructive', onPress: () => performDelete(id) }
        ]
      );
      return;
    }

    await performDelete(id);
  };

  const performDelete = async (id: string) => {
    try {
      const response = await fetch(`${apiUrl}/schedules/${id}`, {
        method: 'DELETE'
      });

      if (!response.ok) throw new Error('Failed to delete schedule');

      schedules.setRetry(Math.random());
      Toast.show({
        type: 'success',
        text1: 'Success',
        text2: 'Schedule deleted successfully'
      });
    } catch (error: any) {
      Toast.show({
        type: 'error',
        text1: 'Error',
        text2: error.message || 'Failed to delete schedule'
      });
    }
  };

  const formatTime = (hour: number, minute: number) => {
    return `${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}`;
  };

  const formatDays = (days: number[]) => {
    return days.map(day => DAYS_OF_WEEK[day].label).join(', ');
  };

  const SchedulingStatusCard = () => (
    <Card className="animate-fade-in shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader>
        <CardTitle className="flex-row items-center gap-3">
          <View className="p-2 bg-info/10 rounded-full">
            <Settings className="text-info" width={20} height={20} />
          </View>
          <Text className="text-xl font-bold">Scheduling System</Text>
        </CardTitle>
        <CardDescription>
          Control automatic preset scheduling
        </CardDescription>
      </CardHeader>
      <CardContent>
        <View className="flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
          <View className="flex-row items-center gap-3">
            <StatusIndicator
              status={schedulingStatus.data?.enabled ? 'active' : 'inactive'}
              size="md"
            />
            <View>
              <Text className="text-lg font-semibold">
                {schedulingStatus.data?.enabled ? 'Active' : 'Inactive'}
              </Text>
              <Text className="text-sm text-muted-foreground">
                Scheduling is {schedulingStatus.data?.enabled ? 'enabled' : 'disabled'}
              </Text>
            </View>
          </View>
          <Switch
            checked={schedulingStatus.data?.enabled || false}
            onCheckedChange={toggleScheduling}
          />
        </View>
        {(schedulingStatus.data?.enabled && schedulingStatus.data?.active_preset) ? (
          <View className="mt-3 p-3 bg-success/10 rounded-lg">
            <Text className="text-sm font-medium text-success">
              Active preset: {schedulingStatus.data.active_preset}
            </Text>
          </View>
        ) : null}
      </CardContent>
    </Card>
  );

  const ScheduleCard = ({ schedule }: { schedule: Schedule }) => (
    <Card className="animate-scale-in shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
      <CardHeader className="pb-3">
        <View className="flex-row items-start justify-between">
          <View className="flex-1">
            <CardTitle className="text-lg">{schedule.name}</CardTitle>
            <CardDescription className="mt-1">
              Preset: {schedule.preset_id}
            </CardDescription>
          </View>
          <StatusIndicator
            status={schedule.enabled ? 'active' : 'inactive'}
            size="sm"
          />
        </View>
      </CardHeader>
      <CardContent className="pt-0">
        <View className="gap-3">
          <View className="flex-row items-center gap-2">
            <Clock className="text-muted-foreground" width={16} height={16} />
            <Text className="text-sm">
              {formatTime(schedule.start_hour, schedule.start_minute)} - {formatTime(schedule.end_hour, schedule.end_minute)}
            </Text>
          </View>
          <View className="flex-row items-center gap-2">
            <Calendar className="text-muted-foreground" width={16} height={16} />
            <Text className="text-sm">
              {formatDays(schedule.days_of_week)}
            </Text>
          </View>
          <Button
            variant="destructive"
            size="sm"
            onPress={() => deleteSchedule(schedule.id)}
            className="mt-2"
          >
            <View className="flex-row items-center gap-2">
              <Trash2 className="text-destructive-foreground" width={14} height={14} />
              <Text>Delete</Text>
            </View>
          </Button>
        </View>
      </CardContent>
    </Card>
  );

  const AddScheduleForm = () => {
    const presetOptions = presets.data ? Object.keys(presets.data).map((presetId) => ({ value: presetId, label: presetId })) : [];

    return (
      <Card className="animate-fade-in shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
        <CardHeader>
          <CardTitle className="flex-row items-center gap-3">
            <View className="p-2 bg-success/10 rounded-full">
              <Plus className="text-success" width={20} height={20} />
            </View>
            <Text className="text-xl font-bold">Add New Schedule</Text>
          </CardTitle>
        </CardHeader>
        <CardContent className="gap-4">
          <View>
            <Label>Schedule Name</Label>
            <Input
              value={newSchedule.name}
              onChangeText={(text) => setNewSchedule({ ...newSchedule, name: text })}
              placeholder="Enter schedule name"
            />
          </View>

          <View>
            <Label>Preset</Label>
            <Select
              value={presetOptions.find(opt => opt.value === newSchedule.preset_id) || undefined}
              onValueChange={(option) => setNewSchedule({ ...newSchedule, preset_id: option?.value || '' })}
            >
              <SelectTrigger>
                <SelectValue placeholder="Select preset" />
              </SelectTrigger>
              <SelectContent>
                {presetOptions.map((option) => (
                  <SelectItem key={option.value} value={option.value} label={option.label}>
                    {option.label}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </View>

          <View className="flex-row gap-4">
            <View className="flex-1">
              <Label>Start Time</Label>
              <View className="flex-row gap-2">
                <Input
                  value={newSchedule.start_hour?.toString()}
                  onChangeText={(text) => setNewSchedule({ ...newSchedule, start_hour: parseInt(text) || 0 })}
                  placeholder="Hour"
                  className="flex-1"
                  keyboardType="numeric"
                />
                <Input
                  value={newSchedule.start_minute?.toString()}
                  onChangeText={(text) => setNewSchedule({ ...newSchedule, start_minute: parseInt(text) || 0 })}
                  placeholder="Min"
                  className="flex-1"
                  keyboardType="numeric"
                />
              </View>
            </View>
            <View className="flex-1">
              <Label>End Time</Label>
              <View className="flex-row gap-2">
                <Input
                  value={newSchedule.end_hour?.toString()}
                  onChangeText={(text) => setNewSchedule({ ...newSchedule, end_hour: parseInt(text) || 0 })}
                  placeholder="Hour"
                  className="flex-1"
                  keyboardType="numeric"
                />
                <Input
                  value={newSchedule.end_minute?.toString()}
                  onChangeText={(text) => setNewSchedule({ ...newSchedule, end_minute: parseInt(text) || 0 })}
                  placeholder="Min"
                  className="flex-1"
                  keyboardType="numeric"
                />
              </View>
            </View>
          </View>

          <View>
            <Label>Days of Week</Label>
            <View className="flex-row flex-wrap gap-2 mt-2">
              {DAYS_OF_WEEK.map((day) => (
                <Button
                  key={day.value}
                  variant={newSchedule.days_of_week?.includes(day.value) ? "default" : "outline"}
                  size="sm"
                  onPress={() => {
                    const currentDays = newSchedule.days_of_week || [];
                    const newDays = currentDays.includes(day.value)
                      ? currentDays.filter(d => d !== day.value)
                      : [...currentDays, day.value];
                    setNewSchedule({ ...newSchedule, days_of_week: newDays });
                  }}
                >
                  <Text>{day.label}</Text>
                </Button>
              ))}
            </View>
          </View>

          <View className="flex-row gap-2">
            <Button
              variant="outline"
              className="flex-1"
              onPress={() => setShowAddForm(false)}
            >
              <Text>Cancel</Text>
            </Button>
            <Button
              className="flex-1"
              onPress={addSchedule}
              disabled={loading}
            >
              <Text>{loading ? 'Adding...' : 'Add Schedule'}</Text>
            </Button>
          </View>
        </CardContent>
      </Card>
    );
  };

  return (
    <SafeAreaView className="flex-1 bg-background">
      <ScrollView
        className="flex-1"
        contentContainerStyle={{
          padding: 20,
          gap: 20
        }}
      >
        <View className={`w-full ${isWeb ? 'max-w-4xl mx-auto' : ''}`}>
          <SchedulingStatusCard />

          {!showAddForm ? (
            <Button
              onPress={() => setShowAddForm(true)}
              className="mb-4"
            >
              <View className="flex-row items-center gap-2">
                <Plus className="text-primary-foreground" width={20} height={20} />
                <Text>Add New Schedule</Text>
              </View>
            </Button>
          ) : null}

          {showAddForm ? <AddScheduleForm /> : null}

          <View className="gap-4">
            <Text className="text-2xl font-bold">Active Schedules</Text>
            {!schedules.data ? null : (Object.keys(schedules.data).length > 0 ? (
              <View className={`gap-4 ${isWeb ? 'grid grid-cols-1 md:grid-cols-2' : ''}`}>
                {Object.values(schedules.data).map((schedule) => (
                  <ScheduleCard key={schedule.id} schedule={schedule} />
                ))}
              </View>
            ) : (
              <Card className="border-dashed border-muted-foreground/50">
                <CardContent className="py-8">
                  <View className="items-center gap-3">
                    <Calendar className="text-muted-foreground" width={48} height={48} />
                    <Text className="text-lg font-semibold text-muted-foreground">
                      No schedules found
                    </Text>
                    <Text className="text-sm text-muted-foreground text-center">
                      Create your first schedule to automatically switch presets
                    </Text>
                  </View>
                </CardContent>
              </Card>
            ))}
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
}
