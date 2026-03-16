import { useState } from 'react'
import { ChevronDown, ChevronUp, Trash2, ArrowRight } from 'lucide-react'
import { useNavigate } from 'react-router-dom'
import { Card, CardContent, CardHeader } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
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
                <div className="flex items-center gap-2">
                  <Badge variant="secondary" className="font-mono text-xs">
                    {scene.type}
                  </Badge>
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
            <CardContent className="pt-0 pb-4 px-4 space-y-3">
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
                  Edit Providers
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
