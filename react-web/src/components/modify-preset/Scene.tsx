import { useState } from 'react'
import { ChevronDown, ChevronUp, Trash2, ArrowRight, ImageOff, SlidersHorizontal } from 'lucide-react'
import { useNavigate } from 'react-router-dom'
import { Card, CardContent, CardHeader } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import {
  AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent,
  AlertDialogDescription, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle
} from '~/components/ui/alert-dialog'
import {
  Collapsible, CollapsibleContent, CollapsibleTrigger
} from '~/components/ui/collapsible'
import type { Scene as SceneType } from '~/apiTypes/list_presets'
import type { ListScenes, ListProviders } from '~/apiTypes/list_scenes'
import { useSceneContext } from './SceneContext'
import PropertyList from './property_list'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import { Badge } from '../ui/badge'
import { sceneArgumentsForVariant, sceneVariant } from '~/lib/sceneVariants'

interface SceneProps {
  scene: SceneType
  sceneDefinitions: ListScenes[]
  providers: ListProviders[]
  presetId: string
}

export default function Scene({ scene, sceneDefinitions, providers, presetId }: SceneProps) {
  const [open, setOpen] = useState(false)
  const [confirmDelete, setConfirmDelete] = useState(false)
  const { deleteScene, updateScene } = useSceneContext()
  const navigate = useNavigate()
  const apiUrl = useApiUrl()

  const def = sceneDefinitions.find(s => s.name === scene.type)

  const hasProviders = def?.properties.some(p =>
    p.type_id === 'json' && (
      (p.additional?.provider_type === 'image') || (p.additional?.provider_type === 'shader')
    )
  )

  const hasShaderProviders = def?.properties.some(p =>
    p.type_id === 'json' && p.additional?.provider_type === 'shader'
  )

  const handleProviderEdit = () => {
    const route = hasShaderProviders
      ? `/modify-shader-providers/${encodeURIComponent(presetId)}/${scene.uuid}`
      : `/modify-providers/${encodeURIComponent(presetId)}/${scene.uuid}`
    navigate(route)
  }

  return (
    <>
      <Card>
        <Collapsible open={open} onOpenChange={setOpen}>
          <CollapsibleTrigger asChild>
            <CardHeader className="cursor-pointer hover:bg-secondary/30 transition-colors rounded-t-xl p-4">
              <div className="flex items-center justify-between">
                <div className="flex min-w-0 items-center gap-3">
                  <div className="grid h-12 w-12 shrink-0 place-items-center overflow-hidden rounded-lg bg-black text-white/35">
                    {def?.has_preview ? <img src={`${apiUrl}/scene_preview?name=${encodeURIComponent(scene.type)}`} alt="" className="h-full w-full object-contain [image-rendering:pixelated]" /> : <ImageOff className="h-4 w-4" />}
                  </div>
                  <div className="min-w-0"><div className="truncate font-semibold">{scene.type}</div><div className="text-xs text-muted-foreground">{def?.category ?? 'Unknown'} · {def?.properties.length ?? 0} settings</div></div>
                </div>
                <div className="flex items-center gap-1">
                  <Button
                    variant="ghost"
                    size="icon"
                    className="h-7 w-7 text-muted-foreground hover:text-destructive"
                    onClick={(e) => { e.stopPropagation(); setConfirmDelete(true) }}
                  >
                    <Trash2 className="h-3.5 w-3.5" />
                  </Button>
                  {open ? <ChevronUp className="h-4 w-4 text-muted-foreground" /> : <ChevronDown className="h-4 w-4 text-muted-foreground" />}
                </div>
              </div>
            </CardHeader>
          </CollapsibleTrigger>
          <CollapsibleContent>
            <CardContent className="space-y-4 border-t border-border/60 px-4 pb-4 pt-4 sm:px-5 sm:pb-5">
              <div className="flex items-center justify-between gap-3">
                <div className="flex items-center gap-2">
                  <div className="grid h-8 w-8 place-items-center rounded-lg bg-primary/10 text-primary">
                    <SlidersHorizontal className="h-4 w-4" />
                  </div>
                  <div>
                    <h3 className="text-sm font-semibold">Scene settings</h3>
                    <p className="text-xs text-muted-foreground">Changes are kept as drafts until you save the preset.</p>
                  </div>
                </div>
                <Badge variant="secondary">{def?.properties.length ?? 0}</Badge>
              </div>
              {def && (def.descriptor?.variants.length ?? 0) > 0 && <div className="rounded-xl border border-border/70 bg-secondary/20 p-3">
                <div className="mb-2 flex items-center justify-between gap-3"><div><div className="text-sm font-medium">Curated look</div><div className="text-xs text-muted-foreground">Switching looks resets this scene's settings to that curated baseline.</div></div>{scene.variant && <Badge variant="outline">{sceneVariant(def, scene.variant)?.label ?? scene.variant}</Badge>}</div>
                <select className="flex h-10 w-full rounded-md border border-input bg-background px-3 text-sm" value={scene.variant ?? ''} onChange={e => { const variant = e.target.value; updateScene(scene.uuid, { ...scene, variant: variant || undefined, arguments: sceneArgumentsForVariant(def, variant) }) }}>
                  <option value="">Original</option>{def.descriptor?.variants.map(variant => <option key={variant.id} value={variant.id}>{variant.label}</option>)}
                </select>
              </div>}
              {def ? (
                <PropertyList
                  properties={def.properties}
                  arguments={scene.arguments}
                  providers={providers}
                  onChange={(args) => updateScene(scene.uuid, { ...scene, arguments: args })}
                />
              ) : (
                <p className="text-sm text-muted-foreground">Unknown scene type: {scene.type}</p>
              )}
              {hasProviders && (
                <Button
                  variant="outline"
                  size="sm"
                  className="gap-2 w-full"
                  onClick={handleProviderEdit}
                >
                  Configure sources
                  <ArrowRight className="h-4 w-4" />
                </Button>
              )}
            </CardContent>
          </CollapsibleContent>
        </Collapsible>
      </Card>

      <AlertDialog open={confirmDelete} onOpenChange={setConfirmDelete}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Delete scene?</AlertDialogTitle>
            <AlertDialogDescription>
              Remove the "{scene.type}" scene from this preset?
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction
              className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
              onClick={() => deleteScene(scene.uuid)}
            >
              Delete
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>
    </>
  )
}
