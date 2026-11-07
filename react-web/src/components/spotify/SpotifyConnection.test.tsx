import { afterEach, describe, expect, it, vi } from 'vitest'
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react'
import { ApiUrlContext } from '~/components/apiUrl/apiUrlContext'
import SpotifyConnection from './SpotifyConnection'

const initial = { configured: false, connected: false, client_id: '', redirect_uri: 'https://spotify-auth.sshcrack.me/callback' }
const response = (data: unknown, status = 200) => new Response(JSON.stringify(data), { status })
function mount(settings = false) { return render(<ApiUrlContext.Provider value="http://matrix.test"><SpotifyConnection settings={settings} /></ApiUrlContext.Provider>) }
afterEach(() => { localStorage.clear(); cleanup(); vi.unstubAllGlobals(); vi.restoreAllMocks() })

describe('Spotify web setup', () => {
  it('prompts on a fresh install and saves the Client ID before enabling connect', async () => {
    const fetcher = vi.fn().mockResolvedValueOnce(response(initial))
      .mockResolvedValueOnce(response({ success: true }))
      .mockResolvedValueOnce(response({ ...initial, configured: true, client_id: 'a'.repeat(32) }))
    vi.stubGlobal('fetch', fetcher)
    mount()
    const id = await screen.findByLabelText('Client ID')
    expect((screen.getByRole('button', { name: 'Connect Spotify' }) as HTMLButtonElement).disabled).toBe(true)
    fireEvent.change(id, { target: { value: 'a'.repeat(32) } })
    expect(screen.queryByLabelText('Client secret')).toBeNull() // PKCE: no secret needed
    fireEvent.click(screen.getByRole('button', { name: 'Save Client ID' }))
    await screen.findByText('Client ID saved. Connect your Spotify account next.')
    expect(JSON.parse(fetcher.mock.calls[1][1].body)).toEqual({ client_id: 'a'.repeat(32) })
    expect((screen.getByRole('button', { name: 'Connect Spotify' }) as HTMLButtonElement).disabled).toBe(false)
  })

  it('keeps the entered Client ID and shows actionable server errors when saving fails', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValueOnce(response(initial))
      .mockResolvedValueOnce(response({ error: 'Config directory is not writable.' }, 400)))
    mount()
    fireEvent.change(await screen.findByLabelText('Client ID'), { target: { value: 'id' } })
    fireEvent.click(screen.getByRole('button', { name: 'Save Client ID' }))
    expect((await screen.findByRole('alert')).textContent).toContain('Config directory is not writable.')
    expect((screen.getByLabelText('Client ID') as HTMLInputElement).value).toBe('id')
  })

  it('does not show Spotify setup when the plugin is absent', async () => {
    const fetcher = vi.fn().mockResolvedValue(response({}, 404))
    vi.stubGlobal('fetch', fetcher)
    const { container } = mount()
    await waitFor(() => expect(fetcher).toHaveBeenCalled())
    expect(container.textContent).toBe('')
  })

  it('remembers dismissal after a reload and allows setup from Integrations later', async () => {
    vi.stubGlobal('fetch', vi.fn().mockImplementation(() => Promise.resolve(response(initial))))
    const first = mount()
    fireEvent.click(await screen.findByRole('button', { name: 'Dismiss setup' }))
    expect(screen.queryByRole('region', { name: 'Spotify connection' })).toBeNull()
    first.unmount()

    const reloaded = mount()
    await waitFor(() => expect(fetch).toHaveBeenCalledTimes(2))
    expect(screen.queryByLabelText('Client ID')).toBeNull()
    reloaded.unmount()

    mount(true)
    expect(await screen.findByLabelText('Client ID')).toBeTruthy()
    expect(screen.queryByRole('button', { name: 'Dismiss setup' })).toBeNull()
  })

  it('explains unavailable plugins on the Integrations page', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(response({}, 404)))
    mount(true)
    await screen.findByText('Spotify is not installed on this matrix. Install the SpotifyScenes plugin to use this integration.')
  })

  it('reports blocked sign-in popups and preserves the connection guide', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValueOnce(response({ ...initial, configured: true }))
      .mockResolvedValueOnce(response({ url: 'https://accounts.spotify.com/authorize?state=test' })))
    vi.spyOn(window, 'open').mockReturnValue(null)
    mount()
    fireEvent.click(await screen.findByRole('button', { name: 'Continue setup' }))
    fireEvent.click(screen.getByRole('button', { name: 'Connect Spotify' }))
    expect((await screen.findByRole('alert')).textContent).toContain('Allow pop-ups')
  })
})
