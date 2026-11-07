import { afterEach, describe, expect, it, vi } from 'vitest'
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react'
import { ApiUrlContext } from '~/components/apiUrl/apiUrlContext'
import SpotifyConnection from './SpotifyConnection'

const initial = { configured: false, connected: false, client_id: '', redirect_uri: 'http://127.0.0.1:8080/spotify/callback' }
const response = (data: unknown, status = 200) => new Response(JSON.stringify(data), { status })
function mount() { return render(<ApiUrlContext.Provider value="http://matrix.test"><SpotifyConnection /></ApiUrlContext.Provider>) }
afterEach(() => { cleanup(); vi.unstubAllGlobals(); vi.restoreAllMocks() })

describe('Spotify web setup', () => {
  it('prompts on a fresh install and saves credentials before enabling connect', async () => {
    const fetcher = vi.fn().mockResolvedValueOnce(response(initial))
      .mockResolvedValueOnce(response({ success: true }))
      .mockResolvedValueOnce(response({ ...initial, configured: true, client_id: 'a'.repeat(32) }))
    vi.stubGlobal('fetch', fetcher)
    mount()
    const id = await screen.findByLabelText('Client ID')
    expect((screen.getByRole('button', { name: 'Connect Spotify' }) as HTMLButtonElement).disabled).toBe(true)
    fireEvent.change(id, { target: { value: 'a'.repeat(32) } })
    fireEvent.change(screen.getByLabelText('Client secret'), { target: { value: 'b'.repeat(32) } })
    fireEvent.click(screen.getByRole('button', { name: 'Save credentials' }))
    await screen.findByText('App credentials saved. Connect your Spotify account next.')
    expect(JSON.parse(fetcher.mock.calls[1][1].body)).toEqual({ client_id: 'a'.repeat(32), client_secret: 'b'.repeat(32) })
    expect((screen.getByLabelText('Client secret') as HTMLInputElement).value).toBe('')
    expect((screen.getByRole('button', { name: 'Connect Spotify' }) as HTMLButtonElement).disabled).toBe(false)
  })

  it('keeps entered credentials and shows actionable server errors when saving fails', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValueOnce(response(initial))
      .mockResolvedValueOnce(response({ error: 'Config directory is not writable.' }, 400)))
    mount()
    fireEvent.change(await screen.findByLabelText('Client ID'), { target: { value: 'id' } })
    fireEvent.change(screen.getByLabelText('Client secret'), { target: { value: 'secret' } })
    fireEvent.click(screen.getByRole('button', { name: 'Save credentials' }))
    expect((await screen.findByRole('alert')).textContent).toContain('Config directory is not writable.')
    expect((screen.getByLabelText('Client secret') as HTMLInputElement).value).toBe('secret')
  })

  it('does not show Spotify setup when the plugin is absent', async () => {
    const fetcher = vi.fn().mockResolvedValue(response({}, 404))
    vi.stubGlobal('fetch', fetcher)
    const { container } = mount()
    await waitFor(() => expect(fetcher).toHaveBeenCalled())
    expect(container.textContent).toBe('')
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
