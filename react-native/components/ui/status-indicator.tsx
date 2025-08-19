import * as React from 'react';
import { View } from 'react-native';
import Animated, { useSharedValue, useAnimatedStyle, withRepeat, withTiming, Easing } from 'react-native-reanimated';
import { cn } from '~/lib/utils';

interface StatusIndicatorProps {
  status: 'active' | 'inactive' | 'pending' | 'error';
  size?: 'sm' | 'md' | 'lg';
  className?: string;
}

const StatusIndicator = ({ status, size = 'md', className }: StatusIndicatorProps) => {
  const sizeClasses = {
    sm: 'w-2 h-2',
    md: 'w-3 h-3',
    lg: 'w-4 h-4'
  };

  // Remove animation classes, keep only color classes
  const statusClasses = {
    active: 'bg-success',
    inactive: 'bg-muted-foreground',
    pending: 'bg-warning',
    error: 'bg-destructive'
  };

  // Animation logic
  const pulse = useSharedValue(1);

  React.useEffect(() => {
    if (status === 'active' || status === 'pending' || status === 'error') {
      pulse.value = withRepeat(
        withTiming(1.5, { duration: 700, easing: Easing.inOut(Easing.ease) }),
        -1,
        true
      );
    } else {
      pulse.value = 1;
    }
  }, [status]);

  const animatedStyle = useAnimatedStyle(() => ({
    transform: [{ scale: pulse.value }]
  }));

  return (
    <Animated.View
      className={cn(
        'rounded-full',
        sizeClasses[size],
        statusClasses[status],
        className
      )}
      style={animatedStyle}
    />
  );
};

export { StatusIndicator };
