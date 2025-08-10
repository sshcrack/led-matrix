import React from 'react';
import { View } from 'react-native';
import { Card, CardContent } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Button } from '~/components/ui/button';
import { Activity } from '~/lib/icons/Activity';

interface ErrorCardProps {
  error: any;
  setRetry: () => void;
}

const ErrorCard: React.FC<ErrorCardProps> = ({ error, setRetry }) => (
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
          {error?.message || "Unable to connect to LED matrix"}
        </Text>
        <Button onPress={setRetry} className="mt-2">
          <Text>Retry Connection</Text>
        </Button>
      </View>
    </CardContent>
  </Card>
);

export default ErrorCard;
