import { Save, Loader2 } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { useConfig } from './ConfigProvider'

export function SaveButton() {
  const { save, isSaving, isDirty } = useConfig()

  return (
    <Button
      onClick={save}
      disabled={!isDirty || isSaving}
      size="sm"
      className="gap-2"
    >
      {isSaving ? (
        <Loader2 className="h-4 w-4 animate-spin" />
      ) : (
        <Save className="h-4 w-4" />
      )}
      {isSaving ? 'Saving...' : 'Save'}
    </Button>
  )
}
