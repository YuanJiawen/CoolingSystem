// 运行冷却控制模块测试的 wasm 产物:输出失败数/首个失败行号,以退出码报告
'use strict';

const fs = require('fs');

const wasmPath = process.argv[2];
if (!wasmPath) {
    console.error('usage: node cooling_control_test.js <test.wasm>');
    process.exit(2);
}

const buf = fs.readFileSync(wasmPath);

WebAssembly.instantiate(buf, {}).then(({ instance }) => {
    const fails = instance.exports.run_tests();
    const lineAddr = instance.exports.g_last_fail_line.value;
    const line = new DataView(instance.exports.memory.buffer).getInt32(lineAddr, true);
    if (fails > 0) {
        console.error(`FAIL: ${fails} assertion(s) failed, last at harness line ${line}`);
        process.exit(1);
    }
    console.log('PASS: all cooling control assertions passed');
    process.exit(0);
}).catch((err) => {
    console.error('RUN ERROR: ' + err.message);
    process.exit(2);
});
