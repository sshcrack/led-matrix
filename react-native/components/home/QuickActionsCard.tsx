import React from 'react';
import { View } from 'react-native';
import { Link } from 'expo-router';
import { Button } from '~/components/ui/button';
import { Card, CardHeader, CardTitle, CardContent } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Badge } from '~/components/ui/badge';
import { Activity } from '~/lib/icons/Activity';
import { Calendar } from '~/lib/icons/Calendar';
import { Download } from '~/lib/icons/Download';
import { Settings } from '~/lib/icons/Settings';
import { FilePlus2 } from '~/lib/icons/FilePlus2';
import { UpdateStatus } from '~/components/apiTypes/update';

interface QuickActionsCardProps {
    isWeb: boolean;
    updateStatus: { data: UpdateStatus | null };
    setRetry: () => void;
}

const QuickActionsCard: React.FC<QuickActionsCardProps> = ({ isWeb, updateStatus, setRetry }) => (
    <Card className="w-full shadow-lg border-0 bg-gradient-to-br from-card to-card/80">
        <CardHeader className="pb-4">
            <View className='w-full flex flex-row items-center justify-between'>
                <View className="flex flex-row items-center gap-3">
                    <View className="p-2 bg-info/10 rounded-full">
                        <Activity className="text-info" width={20} height={20} />
                    </View>
                    <Text className="text-xl font-bold">Quick Actions</Text>
                </View>
            </View>
        </CardHeader>
        <CardContent className="pt-0">
            <View className={`flex flex-row gap-3 ${isWeb ? 'justify-start' : 'justify-between'}`}>
                <Link href="/schedules" asChild>
                    <Button variant="outline" className="flex-1 max-w-48 h-16">
                        <View className="flex flex-row items-center gap-2">
                            <Calendar className="text-foreground" width={20} height={20} />
                            <Text className="text-sm font-medium">Schedules</Text>
                        </View>
                    </Button>
                </Link>
                <Link href="/updates" asChild>
                    <Button variant="outline" className="flex-1 max-w-48 h-16">
                        <View className="flex flex-row items-center gap-2">
                            <View className="relative">
                                <Download className="text-foreground" width={20} height={20} />
                                {updateStatus.data?.update_available ? (
                                    <View className="absolute -top-1 -right-1 w-3 h-3 bg-red-500 rounded-full" />
                                ) : null}
                            </View>
                            <View className="flex flex-col items-start">
                                <Text className="text-sm font-medium">Updates</Text>
                                {updateStatus.data?.update_available ? (
                                    <Badge variant="destructive" className="text-xs px-1 py-0 h-4">
                                        New
                                    </Badge>
                                ) : null}
                            </View>
                        </View>
                    </Button>
                </Link>
                <Link href="/plugins" asChild>
                    <Button variant="outline" className="flex-1 max-w-48 h-16">
                        <View className="flex flex-row items-center gap-2">
                            <FilePlus2 className="text-foreground" width={20} height={20} />
                            <Text className="text-sm font-medium">Plugins</Text>
                        </View>
                    </Button>
                </Link>
            </View>
        </CardContent>
    </Card>
);

export default QuickActionsCard;
