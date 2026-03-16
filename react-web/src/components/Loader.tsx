import { Loader2 } from 'lucide-react'
import { cn } from '~/lib/utils'

interface LoaderProps {
  className?: string
  size?: 'sm' | 'md' | 'lg'
}

export default function Loader({ className, size = 'md' }: LoaderProps) {
  return (
    <div className={cn('flex items-center justify-center', className)}>
      <Loader2
        className={cn(
          'animate-spin text-primary',
          size === 'sm' && 'h-4 w-4',
          size === 'md' && 'h-8 w-8',
          size === 'lg' && 'h-12 w-12'
        )}
      />
    </div>
  )
}
