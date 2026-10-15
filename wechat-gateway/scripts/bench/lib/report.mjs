// 报告渲染：逐场景打印断言 + 统计，汇总 PASS/FAIL，可选写 JSON。
import fs from 'node:fs';

export function render(results, { outJsonPath } = {}) {
  let allPass = true;
  for (const sc of results) {
    console.log(`\n=== ${sc.name} ===`);
    for (const a of sc.asserts) {
      if (!a.pass) allPass = false;
      console.log(`  ${a.pass ? 'PASS' : 'FAIL'}  ${a.label}${a.detail ? `   [${a.detail}]` : ''}`);
    }
    for (const s of sc.stats || []) {
      const v = typeof s.value === 'number' ? (Number.isInteger(s.value) ? s.value : s.value.toFixed(2)) : s.value;
      console.log(`  STAT  ${s.label}: ${v}${s.detail ? `   (${s.detail})` : ''}`);
    }
  }

  const asserts = results.flatMap((s) => s.asserts);
  const pass = asserts.filter((a) => a.pass).length;
  console.log(`\n===== 汇总：断言 PASS ${pass}/${asserts.length} ${allPass ? '✅' : '❌'} =====`);

  if (outJsonPath) {
    fs.writeFileSync(outJsonPath, JSON.stringify(results, null, 2));
    console.log(`报告 JSON 已写入 ${outJsonPath}`);
  }
  return allPass;
}
