// 前端替身：连网关 /ws，计数 device_update / cmd_response。
// 用于断言「边侧事件经网关透传到前端」这条链路的送达，替代真实浏览器观察。
export class WsObserver {
  constructor(url) {
    this.url = url;
    this.deviceUpdates = 0;
    this.cmdResponses = 0;
    this.others = 0;
    this.errors = [];
    this._ws = null;
  }

  connect(timeoutMs = 8000) {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(this.url);
      this._ws = ws;
      const timer = setTimeout(() => reject(new Error(`WS open timeout: ${this.url}`)), timeoutMs);
      ws.addEventListener('open', () => { clearTimeout(timer); resolve(this); });
      ws.addEventListener('message', (ev) => this._onMessage(ev));
      ws.addEventListener('error', (ev) => this.errors.push(String(ev?.message || ev?.type || 'error')));
      ws.addEventListener('close', () => clearTimeout(timer));
    });
  }

  _onMessage(ev) {
    if (typeof ev.data !== 'string') return;
    try {
      const msg = JSON.parse(ev.data);
      if (msg?.type === 'device_update') this.deviceUpdates++;
      else if (msg?.type === 'cmd_response') this.cmdResponses++;
      else this.others++;
    } catch {
      this.others++;
    }
  }

  close() {
    try { this._ws?.close(); } catch { /* ignore */ }
  }

  // 轮询等待某类消息计数达到阈值；超时返回 false。
  async waitFor(kind, n, timeoutMs = 10000) {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const cur = kind === 'device_update' ? this.deviceUpdates : this.cmdResponses;
      if (cur >= n) return true;
      if (Date.now() >= deadline) return false;
      await new Promise((r) => setTimeout(r, 50));
    }
  }
}
