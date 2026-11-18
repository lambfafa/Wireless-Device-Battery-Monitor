// XIBERIA K03S Wireless 7.1 (36F9:C001), tested on its 2.4 GHz receiver.
// The C-Media CM2025 receiver firmware forwards the 0x65 status query to paired headset 1.
// On FF90:0001, a 64-byte output report [55 65 01 00 ...] asks for battery status.
// Input [55 65 <percent> 02 ...] is the battery field, as in HeadsetControl's MCHOSE X9 driver
// (github.com/Sapd/HeadsetControl/pull/563). Verified against XIBERIA's firmware and a live K03S.
#include "drivers.h"

namespace {

constexpr USHORT kVendorId = 0x36F9;
constexpr USHORT kProductId = 0xC001;
constexpr BYTE kReportId = 0x55;
constexpr BYTE kBatteryCommand = 0x65;
constexpr BYTE kHeadsetIndex = 1;
constexpr BYTE kBatteryField = 0x02;
constexpr int kReportLength = 64;

int ReadBattery(HidDev& device) {
    device.Flush();
    const ULONGLONG deadline = GetTickCount64() + 750;
    if (!device.Write({ kReportId, kBatteryCommand, kHeadsetIndex }, 350)) return -1;

    std::vector<BYTE> report;
    for (;;) {
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) return -1;
        const int count = device.Read(report, static_cast<int>(deadline - now));
        if (count < 0) return -1;
        if (count >= 4 && report[0] == kReportId && report[1] == kBatteryCommand &&
            report[3] == kBatteryField && report[2] <= 100) {
            return report[2];
        }
    }
}

}  // namespace

void PollXiberia(PollContext& ctx) {
    if (ctx.IsClaimed(kVendorId, kProductId)) return;
    for (const HidColl* collection : ctx.Find(kVendorId, kProductId, [](const HidColl& c) {
             return c.iface == 0 && c.page() == 0xFF90 && c.usage() == 0x0001 &&
                    c.inLen() == kReportLength && c.outLen() == kReportLength;
         })) {
        HidDev device;
        if (!device.Open(*collection)) continue;
        const int percent = ReadBattery(device);
        if (percent < 0) continue;

        DeviceStatus status;
        status.key = L"xiberia:36F9:C001";
        status.name = L"XIBERIA K03S Wireless 7.1";
        status.kind = DeviceKind::Headset;
        status.via = L"2.4 GHz receiver";
        status.percent = percent;
        ctx.Add(status, kVendorId, kProductId);
        return;
    }
}
