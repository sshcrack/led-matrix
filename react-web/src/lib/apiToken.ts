// Optional shared secret for the matrix API (LED_MATRIX_API_TOKEN on the Pi).
// Stored as a SameSite=Strict cookie so same-origin <img>, download and WebSocket
// requests carry it automatically; fetches also send it as a Bearer header.

export const TOKEN_COOKIE = 'led_matrix_token'
export const UNAUTHORIZED_EVENT = 'led-matrix:unauthorized'

export function getApiToken(): string | null {
  const match = document.cookie.split(';').map((c) => c.trim()).find((c) => c.startsWith(`${TOKEN_COOKIE}=`))
  return match ? decodeURIComponent(match.slice(TOKEN_COOKIE.length + 1)) : null
}

export function setApiToken(token: string | null) {
  if (token) {
    document.cookie = `${TOKEN_COOKIE}=${encodeURIComponent(token)}; path=/; max-age=31536000; SameSite=Strict`
  } else {
    document.cookie = `${TOKEN_COOKIE}=; path=/; max-age=0; SameSite=Strict`
  }
}

function isApiRequest(url: string): boolean {
  try {
    const target = new URL(url, window.location.href)
    if (target.origin === window.location.origin) return true
    const configured = localStorage.getItem('apiUrl')
    return configured !== null && target.origin === new URL(configured).origin
  } catch {
    return false
  }
}

let installed = false

export function installFetchAuth() {
  if (installed) return
  installed = true
  const originalFetch = window.fetch.bind(window)

  window.fetch = async (input: RequestInfo | URL, init?: RequestInit) => {
    const url = input instanceof Request ? input.url : input.toString()
    const token = getApiToken()
    let nextInit = init
    if (token && isApiRequest(url)) {
      const headers = new Headers(init?.headers ?? (input instanceof Request ? input.headers : undefined))
      if (!headers.has('Authorization')) headers.set('Authorization', `Bearer ${token}`)
      nextInit = { ...init, headers }
    }
    const response = await originalFetch(input, nextInit)
    if (response.status === 401 && isApiRequest(url)) {
      window.dispatchEvent(new Event(UNAUTHORIZED_EVENT))
    }
    return response
  }
}
