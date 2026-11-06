/** Dark by default (it is a lights-off ambient display); an explicit choice in the theme toggle wins. */
export function applyColorScheme() {
  let stored: string | null = null
  try { stored = localStorage.getItem('colorScheme') } catch { /* storage unavailable */ }
  document.documentElement.classList.toggle('dark', stored ? stored === 'dark' : true)
}
