// HS256 JWT 铸造（与 src/utils/src/JwtUtil.cpp 一致；secret 默认 dev-shared-jwt-secret）。
// claims：access token = {user_id, role, exp}；openid_ticket = {openid, token_type, exp}。
import crypto from 'node:crypto';

const b64url = (buf) =>
  Buffer.from(buf)
    .toString('base64')
    .replace(/=+$/, '')
    .replace(/\+/g, '-')
    .replace(/\//g, '_');

function sign(headerJson, payloadJson, secret) {
  const h = b64url(headerJson);
  const p = b64url(payloadJson);
  const s = b64url(crypto.createHmac('sha256', secret).update(`${h}.${p}`).digest());
  return `${h}.${p}.${s}`;
}

function secretOf(secret) {
  return secret ?? process.env.JWT_SECRET ?? 'dev-shared-jwt-secret';
}

// 前端访问令牌（admin/teacher）。
export function mintJwt(userId, role, { secret, ttlSeconds = 259200 } = {}) {
  return sign(
    JSON.stringify({ alg: 'HS256', typ: 'JWT' }),
    JSON.stringify({ user_id: userId, role, exp: Math.floor(Date.now() / 1000) + ttlSeconds }),
    secretOf(secret),
  );
}

// 已过期令牌（exp 在过去，用于 401 负向断言）。
export function mintExpiredJwt(userId, role, { secret } = {}) {
  return mintJwt(userId, role, { secret, ttlSeconds: -60 });
}

// 绑定流程 openid_ticket（5 分钟有效）。
export function mintOpenidTicket(openid, { secret, ttlSeconds = 300 } = {}) {
  return sign(
    JSON.stringify({ alg: 'HS256', typ: 'JWT' }),
    JSON.stringify({ openid, token_type: 'openid_ticket', exp: Math.floor(Date.now() / 1000) + ttlSeconds }),
    secretOf(secret),
  );
}
