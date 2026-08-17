// 运行 test_sdio_nofatal 的 wasm 产物:输出失败数/首个失败行号,以退出码报告
'use strict';

const fs = require('fs');

const wasmPath = process.argv[2];
if (!wasmPath) {
    console.error('usage: node sdio_nofatal_test.js <test.wasm>');
    process.exit(2);
}

const buf = fs.readFileSync(wasmPath);

WebAssembly.instantiate(buf, {}).then(({ instance }) => {
    const fails = instance.exports.run_tests();
    // C 全局变量位于线性内存中,--export 导出的是其地址(wasm 全局 i32)
    const lineAddr = instance.exports.g_last_fail_line.value;
    const line = new DataView(instance.exports.memory.buffer).getInt32(lineAddr, true);
    if (fails > 0) {
        console.error(`FAIL: ${fails} assertion(s) failed, last at harness line ${line}`);
        process.exit(1);
    }
    console.log('PASS: all SD no-card boot assertions passed');
    process.exit(0);
}).catch((err) => {
    console.error('RUN ERROR: ' + err.message);
    process.exit(2);
});
