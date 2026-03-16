import { AlertCircle } from 'lucide-react'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'

interface ErrorCardProps {
  error: Error | string
  onRetry?: () => void
}

export default function ErrorCard({ error, onRetry }: ErrorCardProps) {
  const message = typeof error === 'string' ? error : error.message
  return (
    <Card className="border-destructive/30">
      <CardContent className="p-6 flex flex-col items-center gap-3 text-center">
        <AlertCircle className="h-8 w-8 text-destructive" />
        <div>
          <p className="font-medium text-destructive">Failed to load</p>
          <p className="text-sm text-muted-foreground mt-1">{message || 'An error occurred'}</p>
        </div>
        {onRetry && (
          <Button variant="outline" size="sm" onClick={onRetry}>
            Try again
          </Button>
        )}
      </CardContent>
    </Card>
  )
}
