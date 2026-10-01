// Initial adapter: live85% matched official UI; mouse-off query timed out.
// Request/offset references: noosxe/atk-tool (MIT, Levon Kirakosyan).
// Host API/header reference: JKWTCN/BatteryMonitor v1.4.260908 (MIT).
// See THIRD_PARTY_NOTICES.md and docs/verification.md before enabling.

const PROTOCOL_VERIFIED = true;
const DEVICE_NAME = "VXE R1 星闪版";
const REPORT_LENGTH = 17;

export function Name() { return DEVICE_NAME; }
export function VendorId() { return 0x373B; }
export function ProductId() { return 0x10C9; }

export function MakeBatteryRequest() {
    const packet = new Uint8Array(REPORT_LENGTH);
    packet[0] = 0x08;
    packet[1] = 0x04;
    let sum = 0;
    for (let i = 1; i < 16; ++i) sum += packet[i];
    packet[16] = (0x4D - sum) & 0xFF;
    return packet;
}

// Response framing/checksum/85% confirmed by live captures and official comparison.
// Do not accept a shorter frame, a missing ID, or an arbitrary percentage byte.
// Mouse-off produced no reply; no data is deliberately returned as null.
export function ParseBatteryResponse(response) {
    if (!response || response.length !== REPORT_LENGTH) return null;
    for (let i = 0; i < response.length; ++i) {
        if (!Number.isInteger(response[i]) || response[i] < 0 || response[i] > 255)
            return null;
    }
    if (response[0] !== 0x08 || response[1] !== 0x04 || response[2] !== 0)
        return null;
    let sum = 0;
    for (let i = 1; i < REPORT_LENGTH; ++i) sum += response[i];
    if ((sum & 0xFF) !== 0x4D) return null;
    if (response[6] > 100) return null;
    // Charging is intentionally omitted until physically verified.
    return { name: DEVICE_NAME, percentage: response[6] };
}

export function GetBattery() {
    // Fail closed: no unvalidated protocol traffic or unverified UI value.
    if (!PROTOCOL_VERIFIED) return null;
    try {
        if (device.productId() !== ProductId() || device.usagePage() !== 0xFF02
            || device.usage() !== 2) return null;
        // The host does not expose report lengths/IDs. Both FF02 collections
        // have Usage 2; col05 is the captured Report 08 collection only.
        // Re-check descriptor/collection mapping after any firmware change.
        if (!/vid_373b&pid_10c9&mi_01&col05#/i.test(device.path())) return null;
        const request = MakeBatteryRequest();
        if (device.write(request, REPORT_LENGTH) !== REPORT_LENGTH) return null;
        return ParseBatteryResponse(device.read(REPORT_LENGTH, 500));
    } catch (_) {
        return null;
    }
}
