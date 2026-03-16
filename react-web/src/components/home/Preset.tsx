import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Play, Pencil, Trash2, MoreVertical, Layers } from 'lucide-react'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import {
  DropdownMenu, DropdownMenuContent, DropdownMenuItem,
  DropdownMenuSeparator, DropdownMenuTrigger
} from '~/components/ui/dropdown-menu'
import {
  AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent,
  AlertDialogDescription, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle,
} from '~/components/ui/alert-dialog'
import type { RawPreset } from '~/apiTypes/list_presets'

interface PresetCardProps {
  name: string
  preset: RawPreset
  isActive: boolean
  onActivate: (name: string) => void
  onDelete: (name: string) => void
}

export default function PresetCard({ name, preset, isActive, onActivate, onDelete }: PresetCardProps) {
  const navigate = useNavigate()
  const [confirmDelete, setConfirmDelete] = useState(false)

  return (
    <>
      <Card className={`transition-all ${isActive ? 'border-primary/50 bg-primary/5' : ''}`}>
        <CardContent className="p-4">
          <div className="flex items-start justify-between gap-2">
            <div className="flex-1 min-w-0">
              <div className="flex items-center gap-2 flex-wrap">
                <h3 className="font-medium truncate">{name}</h3>
                {isActive && (
                  <Badge variant="default" className="text-xs shrink-0">Active</Badge>
                )}
              </div>
              <div className="flex items-center gap-1.5 mt-1 text-muted-foreground">
                <Layers className="h-3.5 w-3.5" />
                <span className="text-xs">{preset.scenes.length} scene{preset.scenes.length !== 1 ? 's' : ''}</span>
              </div>
            </div>
            <div className="flex items-center gap-1 shrink-0">
              {!isActive && (
                <Button
                  variant="ghost"
                  size="icon"
                  className="h-8 w-8"
                  onClick={() => onActivate(name)}
                  title="Activate"
                >
                  <Play className="h-4 w-4" />
                </Button>
              )}
              <DropdownMenu>
                <DropdownMenuTrigger asChild>
                  <Button variant="ghost" size="icon" className="h-8 w-8">
                    <MoreVertical className="h-4 w-4" />
                  </Button>
                </DropdownMenuTrigger>
                <DropdownMenuContent align="end">
                  {!isActive && (
                    <DropdownMenuItem onClick={() => onActivate(name)}>
                      <Play className="h-4 w-4 mr-2" />
                      Activate
                    </DropdownMenuItem>
                  )}
                  <DropdownMenuItem onClick={() => navigate(`/modify-preset/${encodeURIComponent(name)}`)}>
                    <Pencil className="h-4 w-4 mr-2" />
                    Edit
                  </DropdownMenuItem>
                  <DropdownMenuSeparator />
                  <DropdownMenuItem
                    className="text-destructive focus:text-destructive"
                    onClick={() => setConfirmDelete(true)}
                  >
                    <Trash2 className="h-4 w-4 mr-2" />
                    Delete
                  </DropdownMenuItem>
                </DropdownMenuContent>
              </DropdownMenu>
            </div>
          </div>
        </CardContent>
      </Card>

      <AlertDialog open={confirmDelete} onOpenChange={setConfirmDelete}>
        <AlertDialogContent>
          <AlertDialogHeader>
            <AlertDialogTitle>Delete preset?</AlertDialogTitle>
            <AlertDialogDescription>
              This will permanently delete "{name}". This action cannot be undone.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction
              className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
              onClick={() => { onDelete(name); setConfirmDelete(false) }}
            >
              Delete
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>
    </>
  )
}
