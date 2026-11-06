import { useState } from 'react'
import { Copy, MoreVertical, Pencil, Play, Trash2, Type } from 'lucide-react'
import { useNavigate } from 'react-router-dom'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { DropdownMenu, DropdownMenuContent, DropdownMenuItem, DropdownMenuSeparator, DropdownMenuTrigger } from '~/components/ui/dropdown-menu'
import { AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent, AlertDialogDescription, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle } from '~/components/ui/alert-dialog'
import { Dialog, DialogContent, DialogFooter, DialogHeader, DialogTitle } from '~/components/ui/dialog'
import { useMatrix } from '../matrixContext'

/** ⋯ menu for a preset: play, edit, rename, duplicate, delete. `onEdit` lets a variant edit in place instead of navigating. */
export function PresetMenu({ id, onEdit, className }: { id: string; onEdit?: (id: string) => void; className?: string }) {
  const { presets, activate, renamePreset, duplicatePreset, deletePreset, status } = useMatrix()
  const navigate = useNavigate()
  const preset = presets?.[id]
  const [rename, setRename] = useState<string | null>(null)
  const [confirm, setConfirm] = useState(false)
  if (!preset) return null
  const name = preset.display_name || id
  const isCurrent = status?.current === id
  return <>
    <DropdownMenu>
      <DropdownMenuTrigger asChild><Button variant="ghost" size="icon" className={className} aria-label={`Actions for ${name}`}><MoreVertical className="h-4 w-4" /></Button></DropdownMenuTrigger>
      <DropdownMenuContent align="end">
        <DropdownMenuItem onClick={() => void activate(id)}><Play className="mr-2 h-4 w-4" />Play</DropdownMenuItem>
        <DropdownMenuItem onClick={() => onEdit ? onEdit(id) : navigate(`/modify-preset/${encodeURIComponent(id)}`)}><Pencil className="mr-2 h-4 w-4" />Edit</DropdownMenuItem>
        <DropdownMenuItem onClick={() => setRename(name)}><Type className="mr-2 h-4 w-4" />Rename</DropdownMenuItem>
        <DropdownMenuItem onClick={() => void duplicatePreset(id, preset)}><Copy className="mr-2 h-4 w-4" />Duplicate</DropdownMenuItem>
        <DropdownMenuSeparator />
        <DropdownMenuItem disabled={isCurrent} className="text-destructive focus:text-destructive" onClick={() => setConfirm(true)}><Trash2 className="mr-2 h-4 w-4" />{isCurrent ? 'Delete (playing)' : 'Delete'}</DropdownMenuItem>
      </DropdownMenuContent>
    </DropdownMenu>
    <Dialog open={rename !== null} onOpenChange={o => !o && setRename(null)}>
      <DialogContent className="max-w-sm"><DialogHeader><DialogTitle>Rename preset</DialogTitle></DialogHeader>
        <Input autoFocus value={rename ?? ''} onChange={e => setRename(e.target.value)} onKeyDown={e => { if (e.key === 'Enter' && rename?.trim()) { void renamePreset(id, rename.trim()); setRename(null) } }} />
        <DialogFooter><Button disabled={!rename?.trim()} onClick={() => { void renamePreset(id, rename!.trim()); setRename(null) }}>Rename</Button></DialogFooter></DialogContent>
    </Dialog>
    <AlertDialog open={confirm} onOpenChange={setConfirm}>
      <AlertDialogContent><AlertDialogHeader><AlertDialogTitle>Delete “{name}”?</AlertDialogTitle><AlertDialogDescription>This removes the preset and its {preset.scenes.length} scene settings.</AlertDialogDescription></AlertDialogHeader>
        <AlertDialogFooter><AlertDialogCancel>Cancel</AlertDialogCancel><AlertDialogAction className="bg-destructive text-destructive-foreground" onClick={() => void deletePreset(id, name)}>Delete</AlertDialogAction></AlertDialogFooter></AlertDialogContent>
    </AlertDialog>
  </>
}
