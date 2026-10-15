// 零依赖 HTTP 压测工具：单请求封装 + 有界并发池 + 持续负载 + 延迟分位统计。
const DEFAULT_TIMEOUT_MS = 20000;

// 单次请求；body 非空时自动加 Content-Type: application/json。
export async function request({
  method = 'GET',
  url,
  headers = {},
  body,
  timeoutMs = DEFAULT_TIMEOUT_MS,
}) {
  const start = performance.now();
  const hasBody = body !== undefined && body !== null;
  try {
    const res = await fetch(url, {
      method,
      headers: hasBody ? { 'Content-Type': 'application/json', ...headers } : headers,
      body: hasBody ? JSON.stringify(body) : undefined,
      signal: AbortSignal.timeout(timeoutMs),
    });
    const text = await res.text();
    const ms = performance.now() - start;
    let json = null;
    try { json = JSON.parse(text); } catch { /* 非 JSON 响应体 */ }
    return { status: res.status, ms, json, text, ok: res.status >= 200 && res.status < 300 };
  } catch (e) {
    return { status: 0, ms: performance.now() - start, json: null, text: String(e?.message || e), ok: false, error: e };
  }
}

// 有界并发执行 tasks（每个 task 是 async () => result），返回与 tasks 同序的结果数组。
export async function runPool(tasks, concurrency) {
  if (!tasks.length) return [];
  const results = new Array(tasks.length);
  let idx = 0;
  const workers = Array.from({ length: Math.min(concurrency, tasks.length) }, async () => {
    for (;;) {
      const i = idx++;
      if (i >= tasks.length) break;
      results[i] = await tasks[i]();
    }
  });
  await Promise.all(workers);
  return results;
}

// 持续到 deadline：concurrency 个 worker 各循环调用 fn，收集全部结果。
export async function saturate(concurrency, durationMs, fn) {
  const results = [];
  const deadline = performance.now() + durationMs;
  await Promise.all(
    Array.from({ length: concurrency }, async () => {
      while (performance.now() < deadline) results.push(await fn());
    }),
  );
  return results;
}

// 延迟分位（p50/p90/p99 等）。
export function percentiles(values, ...ps) {
  if (!values.length) return Object.fromEntries(ps.map((p) => [p, 0]));
  const sorted = [...values].sort((a, b) => a - b);
  const q = (p) => {
    const idx = (p / 100) * (sorted.length - 1);
    const lo = Math.floor(idx);
    const hi = Math.ceil(idx);
    return lo === hi ? sorted[lo] : sorted[lo] + (sorted[hi] - sorted[lo]) * (idx - lo);
  };
  return Object.fromEntries(ps.map((p) => [p, q(p)]));
}

// 汇总一批请求结果 -> {count, rps, ok, errorRate, statuses, p50/p90/p99}。
export function summarize(results, durationMs) {
  const ms = results.map((r) => r.ms).filter((v) => Number.isFinite(v));
  const statuses = {};
  for (const r of results) statuses[r.status] = (statuses[r.status] || 0) + 1;
  const ok = results.filter((r) => r.ok).length;
  const p = percentiles(ms, 50, 90, 99);
  return {
    count: results.length,
    rps: durationMs > 0 ? results.length / (durationMs / 1000) : 0,
    ok,
    errorRate: results.length ? (results.length - ok) / results.length : 0,
    statuses,
    p50: p[50],
    p90: p[90],
    p99: p[99],
  };
}

export function bearer(jwt) {
  return { Authorization: `Bearer ${jwt}` };
}

export function internal(token) {
  return { 'X-Internal-Token': token };
}
