import { Check, Palette } from 'lucide-react'
import { DropdownMenu, DropdownMenuContent, DropdownMenuItem, DropdownMenuLabel, DropdownMenuSeparator, DropdownMenuTrigger } from '~/components/ui/dropdown-menu'
import { setVariant, useVariantId } from '~/core/variants'
import { getVariant, variantList } from './index'

/** Draft-PR aid: flip between the UI variants live. Remove with the variants you don't pull through. */
export default function VariantSwitcher() {
  const current = useVariantId()
  return <div className="fixed left-1/2 top-1 z-[60] -translate-x-1/2">
    <DropdownMenu>
      <DropdownMenuTrigger className="flex items-center gap-1.5 rounded-full border border-border bg-card/95 px-2.5 py-1 text-[11px] font-semibold text-foreground shadow-md backdrop-blur hover:bg-secondary" aria-label="Switch UI variant">
        <Palette className="h-3.5 w-3.5 text-primary" />{getVariant(current).name}
      </DropdownMenuTrigger>
      <DropdownMenuContent align="center" side="bottom" className="w-72">
        <DropdownMenuLabel>UI variant <span className="font-normal text-muted-foreground">· draft comparison</span></DropdownMenuLabel>
        <DropdownMenuSeparator />
        {variantList().map(v => <DropdownMenuItem key={v.id} onClick={() => setVariant(v.id)} className="items-start gap-2">
          <Check className={`mt-0.5 h-4 w-4 shrink-0 ${v.id === current ? 'opacity-100' : 'opacity-0'}`} />
          <span><span className="block font-medium">{v.name}</span><span className="block text-xs text-muted-foreground">{v.tagline}</span></span>
        </DropdownMenuItem>)}
      </DropdownMenuContent>
    </DropdownMenu>
  </div>
}
