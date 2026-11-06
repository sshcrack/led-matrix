import { describe, expect, it } from 'vitest'
import { LiveFrameDemand } from './liveFrame'

function onScreen() {
  const demand = new LiveFrameDemand(true)
  demand.setInViewport(true)
  return demand
}

describe('LiveFrameDemand', () => {
  it('a connected socket alone is not demand', () => {
    const demand = new LiveFrameDemand(true)
    demand.socketOpened()
    expect(demand.tryRequest()).toBe(false)
    demand.setInViewport(true)
    expect(demand.tryRequest()).toBe(true)
  })

  it('hidden pages and off-screen previews stop requesting', () => {
    const demand = onScreen()
    demand.socketOpened()
    demand.setPageVisible(false)
    expect(demand.tryRequest()).toBe(false)
    demand.setPageVisible(true)
    demand.setInViewport(false)
    expect(demand.tryRequest()).toBe(false)
  })

  it('allows only one request in flight', () => {
    const demand = onScreen()
    demand.socketOpened()
    expect(demand.tryRequest()).toBe(true)
    expect(demand.tryRequest()).toBe(false)
    demand.frameReceived()
    expect(demand.tryRequest()).toBe(true)
  })

  it('a reconnect drops the outstanding request', () => {
    const demand = onScreen()
    demand.socketOpened()
    expect(demand.tryRequest()).toBe(true)
    demand.socketClosed()
    expect(demand.tryRequest()).toBe(false)
    demand.socketOpened()
    expect(demand.tryRequest()).toBe(true)
  })

  it('never requests after dispose', () => {
    const demand = onScreen()
    demand.socketOpened()
    demand.dispose()
    expect(demand.tryRequest()).toBe(false)
  })
})
