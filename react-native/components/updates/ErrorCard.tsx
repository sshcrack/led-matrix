import * as React from 'react';
import { View } from 'react-native';
import { Card, CardContent } from '~/components/ui/card';
import { Text } from '~/components/ui/text';
import { Button } from '~/components/ui/button';
import { AlertCircle } from '~/lib/icons/AlertCircle';

interface ErrorCardProps {
  error: { message?: string } | null;
  onRetry: () => void;
}

export function ErrorCard({ error, onRetry }: ErrorCardProps) {
  return (
    <Card className="w-full shadow-lg border-destructive/20 bg-destructive/5">
      <CardContent className="pt-6">
        <View className="flex items-center gap-4">
          <View className="p-3 bg-destructive/10 rounded-full">
            <AlertCircle className="text-destructive" width={24} height={24} />
          </View>
          <Text className="text-lg font-semibold text-destructive">
            Connection Error
          </Text>
          <Text className="text-center text-muted-foreground">
            {error?.message || "Unable to connect to update service"}
          </Text>
          <Button onPress={onRetry} className="mt-2">
            <Text>Retry Connection</Text>
          </Button>
        </View>
      </CardContent>
    </Card>
  );
}