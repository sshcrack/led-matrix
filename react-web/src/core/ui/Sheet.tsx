import * as D from '@radix-ui/react-dialog'
import { X } from 'lucide-react'
import type { ReactNode } from 'react'
import { cn } from '~/lib/utils'

const side = {
  bottom: 'inset-x-0 bottom-0 max-h-[92vh] rounded-t-3xl border-t sm:inset-x-auto sm:left-1/2 sm:w-full sm:max-w-2xl sm:-translate-x-1/2',
  right: 'inset-y-0 right-0 w-[min(100vw,34rem)] border-l',
  left: 'inset-y-0 left-0 w-[min(88vw,22rem)] border-r',
} as const

/** Edge-anchored panel (bottom sheet on phones, side drawer on desktop). */
export function Sheet({ open, onOpenChange, title, description, side: s = 'bottom', children, className }: {
  open: boolean; onOpenChange: (o: boolean) => void; title: string; description?: string; side?: keyof typeof side; children: ReactNode; className?: string
}) {
  return <D.Root open={open} onOpenChange={onOpenChange}>
    <D.Portal>
      <D.Overlay className="fixed inset-0 z-50 bg-black/55 backdrop-blur-sm data-[state=open]:animate-in data-[state=open]:fade-in-0" />
      <D.Content onOpenAutoFocus={e => e.preventDefault()} className={cn('fixed z-50 flex flex-col gap-3 border-border bg-card p-4 shadow-2xl outline-none sm:p-5', side[s], className)}>
        <div className="flex items-start justify-between gap-3">
          <div><D.Title className="text-lg font-semibold tracking-tight">{title}</D.Title>
            {description ? <D.Description className="text-sm text-muted-foreground">{description}</D.Description> : <D.Description className="sr-only">{title}</D.Description>}</div>
          <D.Close className="rounded-full p-2 text-muted-foreground hover:bg-secondary" aria-label="Close"><X className="h-4 w-4" /></D.Close>
        </div>
        <div className="flex min-h-0 flex-1 flex-col overflow-y-auto">{children}</div>
      </D.Content>
    </D.Portal>
  </D.Root>
}
