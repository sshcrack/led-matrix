import { cn } from '~/lib/utils'

interface StatusIndicatorProps {
  status: 'online' | 'offline' | 'warning' | 'loading'
  className?: string
}

export function StatusIndicator({ status, className }: StatusIndicatorProps) {
  return (
    <span
      className={cn(
        'inline-block h-2.5 w-2.5 rounded-full',
        status === 'online' && 'bg-success',
        status === 'offline' && 'bg-destructive',
        status === 'warning' && 'bg-warning',
        status === 'loading' && 'bg-muted-foreground animate-pulse',
        className
      )}
    />
  )
}
