import { Activity, Calendar, Download, FlaskConical, FolderCode, Grid3x3, Images } from 'lucide-react'
import type { ReactNode } from 'react'

export interface NavItem { to: string; label: string; description: string; icon: ReactNode; primary?: boolean }
const i = 'h-5 w-5'
export const NAV: NavItem[] = [
  { to: '/', label: 'Control', description: 'Power and presets', icon: <Grid3x3 className={i} />, primary: true },
  { to: '/gallery', label: 'Scenes', description: 'Browse and preview', icon: <Images className={i} />, primary: true },
  { to: '/schedules', label: 'Automation', description: 'Schedules', icon: <Calendar className={i} />, primary: true },
  { to: '/scene-lab', label: 'Lab', description: 'Tune on the Pi', icon: <FlaskConical className={i} /> },
  { to: '/assets', label: 'Assets', description: 'Shaders and media', icon: <FolderCode className={i} /> },
  { to: '/updates', label: 'Updates', description: 'System updates', icon: <Download className={i} /> },
  { to: '/diagnostics', label: 'Health', description: 'Live diagnostics', icon: <Activity className={i} /> },
]
export const isActive = (item: NavItem, pathname: string) => item.to === '/' ? pathname === '/' : pathname.startsWith(item.to)
