import * as React from 'react';
import { type VariantProps, cva } from 'class-variance-authority';
import { Text, View } from 'react-native';
import { cn } from '~/lib/utils';

const badgeVariants = cva(
  'inline-flex items-center rounded-full border px-2.5 py-0.5',
  {
    variants: {
      variant: {
        default: 'border-transparent bg-primary text-primary-foreground',
        secondary: 'border-transparent bg-secondary text-secondary-foreground',
        destructive: 'border-transparent bg-destructive text-destructive-foreground',
        success: 'border-transparent bg-green-500 text-white',
        warning: 'border-transparent bg-yellow-500 text-white',
        info: 'border-transparent bg-blue-500 text-white',
        outline: 'text-foreground border-border',
      },
      size: {
        default: 'text-xs font-semibold',
        sm: 'text-xs',
        lg: 'text-sm px-3 py-1',
      },
    },
    defaultVariants: {
      variant: 'default',
      size: 'default',
    },
  }
);

const badgeTextVariants = cva('text-xs font-semibold', {
  variants: {
    variant: {
      default: 'text-primary-foreground',
      secondary: 'text-secondary-foreground',
      destructive: 'text-destructive-foreground',
      success: 'text-white',
      warning: 'text-white',
      info: 'text-white',
      outline: 'text-foreground',
    },
  },
  defaultVariants: {
    variant: 'default',
  },
});

export interface BadgeProps
  extends React.ComponentPropsWithoutRef<typeof View>,
    VariantProps<typeof badgeVariants> {
  children: React.ReactNode;
}

function Badge({ className, variant, size, children, ...props }: BadgeProps) {
  return (
    <View className={cn(badgeVariants({ variant, size }), className)} {...props}>
      <Text className={cn(badgeTextVariants({ variant }))}>{children}</Text>
    </View>
  );
}

export { Badge, badgeVariants };