// 下游（mock-go-backend）送达证据：mock 把每笔请求打印到日志文件，
// 用「日志文件字节长度快照 + 增量」统计网关实际转发到边侧的请求数。
import fs from 'node:fs';

export function snapshotLog(path) {
  try {
    return { path, len: fs.statSync(path).size, missing: false };
  } catch {
    return { path, len: 0, missing: true };
  }
}

// 返回自 prev 快照以来的日志增量文本（日志为 append-only）。
export function deltaLog(prev) {
  try {
    const text = fs.readFileSync(prev.path, 'utf8');
    return text.slice(prev.len);
  } catch {
    return '';
  }
}

export function countMatches(text, re) {
  return (text.match(re) || []).length;
}
