import { Navigate, useParams } from 'react-router-dom'

/** The console edits in place; old /modify-preset links land on the inline editor. */
export default function ConsoleEditor() {
  const { preset_id } = useParams()
  return <Navigate replace to={`/?edit=${encodeURIComponent(preset_id ?? '')}`} />
}
