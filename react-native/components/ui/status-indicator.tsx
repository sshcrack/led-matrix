import * as React from 'react';
import { View } from 'react-native';
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

  const statusClasses = {
    active: 'bg-success animate-pulse-glow',
    inactive: 'bg-muted-foreground',
    pending: 'bg-warning animate-pulse',
    error: 'bg-destructive animate-pulse'
  };

  return (
    <View className={cn(
      'rounded-full',
      sizeClasses[size],
      statusClasses[status],
      className
    )} />
  );
};

export { StatusIndicator };
