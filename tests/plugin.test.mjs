import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import * as plugin from '../plugins/vxe-r1-nearlink.js';

// Generated frames are synthetic. A separate test replays saved real captures.
function frame(percentage = 72) {
    const b = new Uint8Array(17);
    b[0] = 8; b[1] = 4; b[6] = percentage;
    b[16] = (0x4D - 4 - percentage) & 255;
    return b;
}

test('exact device identity', () => {
    assert.equal(plugin.VendorId(), 0x373B);
    assert.equal(plugin.ProductId(), 0x10C9);
    assert.equal(plugin.Name(), 'VXE R1 星闪版');
});
test('only battery query; payload checksum 0x4D', () => {
    const b = plugin.MakeBatteryRequest();
    assert.deepEqual([...b], [8, 4, ...Array(14).fill(0), 0x49]);
    assert.equal(b.slice(1).reduce((a, x) => a + x, 0) & 255, 0x4D);
});
test('synthetic candidate normal and valid zero/full', () => {
    for (const n of [0, 72, 100]) {
        const result = plugin.ParseBatteryResponse(frame(n));
        assert.equal(result.percentage, n);
        assert.equal('charging' in result, false);
    }
});
test('timeout/empty/truncated/oversized rejected', () => {
    for (const b of [null, undefined, [], frame().slice(0, 10), new Uint8Array(18)])
        assert.equal(plugin.ParseBatteryResponse(b), null);
});
test('wrong report/command/status rejected', () => {
    for (const i of [0, 1, 2]) {
        const b = frame(); b[i] = 0xFF;
        assert.equal(plugin.ParseBatteryResponse(b), null);
    }
});
test('out-of-range/non-byte values rejected', () => {
    for (const n of [101, 255, -1, 1.5, NaN]) {
        const b = [...frame()]; b[6] = n;
        assert.equal(plugin.ParseBatteryResponse(b), null);
    }
});
test('corrupted checksum rejected', () => {
    const b = frame(); b[16] ^= 1;
    assert.equal(plugin.ParseBatteryResponse(b), null);
});
test('real hardware captures and timeout replayed without a device', () => {
    for (const name of ['battery-query-01.json', 'battery-query-02.json', 'battery-query-03.json', 'battery-mouse-off.json']) {
        const capture = JSON.parse(readFileSync(new URL('./fixtures/' + name, import.meta.url), 'utf8'));
        const item = capture.interfaces.find(i => 'response_hex' in i);
        assert.ok(item);
        const response = item.response_hex ? Uint8Array.from(item.response_hex.split(' ').map(h => parseInt(h, 16))) : null;
        const parsed = plugin.ParseBatteryResponse(response);
        if (item.status === 'timeout') assert.equal(parsed, null);
        else assert.equal(parsed.percentage, 85);
    }
});
test('missing device API returns null rather than invented zero', () => {
    assert.equal(plugin.GetBattery(), null);
});

// Test transport branches using an IN-MEMORY enabled copy only.
// This never edits/enables the real plugin and never touches hardware.
function simulated(overrides = {}, enabled = true) {
    const calls = [];
    const device = {
        productId: () => 0x10C9, usagePage: () => 0xFF02, usage: () => 2,
        path: () => '\\\\?\\hid#vid_373b&pid_10c9&mi_01&col05#redacted',
        write: (packet, length) => { calls.push(['write', [...packet], length]); return 17; },
        read: (length, timeout) => { calls.push(['read', length, timeout]); return frame(); },
        ...overrides,
    };
    const source = readFileSync(new URL('../plugins/vxe-r1-nearlink.js', import.meta.url), 'utf8')
        .replace(/const PROTOCOL_VERIFIED = (true|false);/, `const PROTOCOL_VERIFIED = ${enabled};`)
        .replaceAll('export function ', 'function ');
    const context = vm.createContext({ device });
    vm.runInContext(source, context);
    return { value: vm.runInContext('GetBattery()', context), calls };
}
test('disabled gate makes no device calls', () => {
    const { value, calls } = simulated({}, false);
    assert.equal(value, null); assert.equal(calls.length, 0);
});
test('simulated single write/read with 500ms ceiling', () => {
    const { value, calls } = simulated();
    assert.equal(value.percentage, 72);
    assert.equal(calls.length, 2);
    assert.deepEqual(calls[1], ['read', 17, 500]);
});
test('unmatched interfaces never written', () => {
    for (const overrides of [{ productId: () => 1 }, { usagePage: () => 0xFF04 },
        { usage: () => 1 }, { path: () => 'hid#vid_373b&pid_10c9&mi_01&col04#x' }]) {
        const { value, calls } = simulated(overrides);
        assert.equal(value, null); assert.equal(calls.length, 0);
    }
});
test('short write, timeout, exception, bad command return null; no retry', () => {
    for (const overrides of [{ write: () => 0 }, { read: () => null },
        { write: () => { throw new Error('busy'); } }, { read: () => new Uint8Array(17) }]) {
        const { value, calls } = simulated(overrides);
        assert.equal(value, null); assert.ok(calls.length <= 2);
    }
});
