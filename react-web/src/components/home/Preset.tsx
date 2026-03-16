import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Play, Pencil, Trash2, MoreVertical, Layers, Type } from 'lucide-react'
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
import {
  Dialog, DialogContent, DialogDescription, DialogFooter,
  DialogHeader, DialogTitle,
} from '~/components/ui/dialog'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import type { RawPreset } from '~/apiTypes/list_presets'

interface PresetCardProps {
  id: string
  displayName: string
  preset: RawPreset
  isActive: boolean
  onActivate: (id: string, displayName: string) => void
  onDelete: (id: string, displayName: string) => void
  onRename: (id: string, displayName: string) => void
}

export default function PresetCard({ id, displayName, preset, isActive, onActivate, onDelete, onRename }: PresetCardProps) {
  const navigate = useNavigate()
  const [confirmDelete, setConfirmDelete] = useState(false)
  const [showRename, setShowRename] = useState(false)
  const [renameValue, setRenameValue] = useState(displayName)

  const handleOpenRename = () => {
    setRenameValue(displayName)
    setShowRename(true)
  }

  const handleRename = () => {
    const nextName = renameValue.trim()
    if (!nextName || nextName === displayName) {
      setShowRename(false)
      return
    }
    onRename(id, nextName)
    setShowRename(false)
  }

  return (
    <>
      <Card className={`transition-all ${isActive ? 'border-primary/50 bg-primary/5' : ''}`}>
        <CardContent className="p-4">
          <div className="flex items-start justify-between gap-2">
            <div className="flex-1 min-w-0">
              <div className="flex items-center gap-2 flex-wrap">
                <h3 className="font-medium truncate">{displayName}</h3>
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
                  onClick={() => onActivate(id, displayName)}
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
                    <DropdownMenuItem onClick={() => onActivate(id, displayName)}>
                      <Play className="h-4 w-4 mr-2" />
                      Activate
                    </DropdownMenuItem>
                  )}
                  <DropdownMenuItem onClick={handleOpenRename}>
                    <Type className="h-4 w-4 mr-2" />
                    Rename
                  </DropdownMenuItem>
                  <DropdownMenuItem onClick={() => navigate(`/modify-preset/${encodeURIComponent(id)}`)}>
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
              This will permanently delete "{displayName}". This action cannot be undone.
            </AlertDialogDescription>
          </AlertDialogHeader>
          <AlertDialogFooter>
            <AlertDialogCancel>Cancel</AlertDialogCancel>
            <AlertDialogAction
              className="bg-destructive text-destructive-foreground hover:bg-destructive/90"
              onClick={() => { onDelete(id, displayName); setConfirmDelete(false) }}
            >
              Delete
            </AlertDialogAction>
          </AlertDialogFooter>
        </AlertDialogContent>
      </AlertDialog>

      <Dialog open={showRename} onOpenChange={setShowRename}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Rename preset</DialogTitle>
            <DialogDescription>Change the display name shown in the app.</DialogDescription>
          </DialogHeader>
          <div className="space-y-2">
            <Label htmlFor={`rename-${id}`}>Display Name</Label>
            <Input
              id={`rename-${id}`}
              value={renameValue}
              onChange={e => setRenameValue(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleRename()}
              autoFocus
            />
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setShowRename(false)}>Cancel</Button>
            <Button onClick={handleRename} disabled={!renameValue.trim() || renameValue.trim() === displayName}>Save</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  )
}
