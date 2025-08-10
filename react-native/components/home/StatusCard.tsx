import React from 'react';
import { View } from 'react-native';
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '~/components/ui/card';
import { StatusIndicator } from '~/components/ui/status-indicator';
import { Switch } from '~/components/ui/switch';
import { Text } from '~/components/ui/text';
import { Power } from '~/lib/icons/Power';

interface StatusCardProps {
    turnedOn: boolean | null;
    settingStatus: boolean;
    setSettingStatus: (v: boolean) => void;
    setTurnedOn: (v: boolean) => void;
}

const StatusCard: React.FC<StatusCardProps> = ({ turnedOn, settingStatus, setSettingStatus, setTurnedOn }) => (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
        <CardHeader className="pb-4">
            <CardTitle className="flex flex-row items-center gap-3">
                <View className="p-2 bg-primary/10 rounded-full">
                    <Power className="text-primary" width={24} height={24} />
                </View>
                <Text className="text-2xl font-bold">Matrix Control</Text>
            </CardTitle>
            <CardDescription className="text-base">
                Manage your LED matrix display system
            </CardDescription>
        </CardHeader>
        <CardContent className="pt-0">
            <View className="flex flex-row items-center justify-between p-4 bg-secondary/30 rounded-xl">
                <View className="flex flex-row items-center gap-3">
                    <StatusIndicator
                        status={turnedOn ? 'active' : 'inactive'}
                        size="lg"
                    />
                    <View>
                        <Text className="text-lg font-semibold">
                            {settingStatus ? "Updating..." : turnedOn ? "Active" : "Inactive"}
                        </Text>
                        <Text className="text-sm text-muted-foreground">
                            Matrix display is {turnedOn ? "enabled" : "disabled"}
                        </Text>
                    </View>
                </View>
                <Switch
                    disabled={turnedOn === null || settingStatus}
                    checked={turnedOn ?? false}
                    onCheckedChange={v => {
                        setSettingStatus(true);
                        setTurnedOn(v);
                    }}
                    nativeID='led-matrix-status'
                />
            </View>
        </CardContent>
    </Card>
);

export default StatusCard;
