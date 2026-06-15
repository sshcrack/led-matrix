# Features

## Design Constraints

The LED matrix is a **passive prop** in a room. It must:

- **Low compute** on the Raspberry Pi — scenes must be lightweight
- **Non-interactive** — no user interaction with the matrix itself (configuration via web UI is fine, but the running display requires no input)
- **No text reading** — no tickers, headlines, scores, or any text the viewer would need to actively read
- **Visually interesting** — the primary goal is ambiance and visual appeal

---

## Rejected Ideas

| Idea | Reason |
|------|--------|
| Home Assistant / MQTT | Not needed |
| WebSocket live preview | Too heavy on the RPi |
| RSS / News Feed | Not used; also violates no-text rule |
| Sports Scores | Not needed; violates no-text rule |
| Crypto / Stock Ticker | Not needed; violates no-text rule |
| Scrolling News Ticker | Violates no-text rule |
| Web Paint / Pixel Canvas | Too interactive |
| Emoji Signal Scene | Implies active triggering from user |

---

## Decision Log

- **2026-06-01**: Initial decisions. Constraints set: low compute, non-interactive, no text, prop use case.
