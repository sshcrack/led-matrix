import { afterEach, beforeAll, beforeEach, describe, expect, it, vi } from 'vitest'
import { getApiToken, installFetchAuth, setApiToken, TOKEN_COOKIE, UNAUTHORIZED_EVENT } from './apiToken'

const underlyingFetch = vi.fn<typeof fetch>()

beforeAll(() => {
  // installFetchAuth wraps whatever window.fetch is at install time (once).
  window.fetch = underlyingFetch
  installFetchAuth()
})

beforeEach(() => {
  underlyingFetch.mockReset()
  underlyingFetch.mockResolvedValue(new Response('{}', { status: 200 }))
  setApiToken(null)
})

afterEach(() => {
  setApiToken(null)
})

function sentHeaders(): Headers {
  const init = underlyingFetch.mock.calls[0][1]
  return new Headers(init?.headers)
}

describe('token cookie', () => {
  it('round-trips a token, including special characters', () => {
    expect(getApiToken()).toBeNull()
    setApiToken('s3cr;et = value/+')
    expect(getApiToken()).toBe('s3cr;et = value/+')
    expect(document.cookie).toContain(`${TOKEN_COOKIE}=`)
  })

  it('clears the token when set to null', () => {
    setApiToken('abc')
    setApiToken(null)
    expect(getApiToken()).toBeNull()
  })
})

describe('installFetchAuth', () => {
  it('adds a Bearer header to same-origin requests', async () => {
    setApiToken('tok')
    await window.fetch('/api/status')
    expect(sentHeaders().get('Authorization')).toBe('Bearer tok')
  })

  it('does not overwrite an explicit Authorization header', async () => {
    setApiToken('tok')
    await window.fetch('/x', { headers: { Authorization: 'Basic abc' } })
    expect(sentHeaders().get('Authorization')).toBe('Basic abc')
  })

  it('does not send the token to third-party origins', async () => {
    setApiToken('tok')
    await window.fetch('https://example.com/data.json')
    expect(underlyingFetch.mock.calls[0][1]).toBeUndefined()
  })

  it('sends no header when no token is stored', async () => {
    await window.fetch('/api/status')
    expect(sentHeaders().has('Authorization')).toBe(false)
  })

  it('dispatches the unauthorized event on a 401 from the API', async () => {
    const listener = vi.fn()
    window.addEventListener(UNAUTHORIZED_EVENT, listener)
    underlyingFetch.mockResolvedValue(new Response('no', { status: 401 }))
    const response = await window.fetch('/api/status')
    window.removeEventListener(UNAUTHORIZED_EVENT, listener)
    expect(response.status).toBe(401)
    expect(listener).toHaveBeenCalledTimes(1)
  })

  it('ignores a 401 from a third-party origin', async () => {
    const listener = vi.fn()
    window.addEventListener(UNAUTHORIZED_EVENT, listener)
    underlyingFetch.mockResolvedValue(new Response('no', { status: 401 }))
    await window.fetch('https://example.com/secret')
    window.removeEventListener(UNAUTHORIZED_EVENT, listener)
    expect(listener).not.toHaveBeenCalled()
  })
})
