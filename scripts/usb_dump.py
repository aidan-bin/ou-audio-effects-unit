"""Dump USB descriptor info for the ou-audio-effects device.

macOS only — uses ioreg to read the device tree.
Parses ioreg output to show device, configuration, interface,
and string descriptors, plus bound drivers and endpoint summary.

Usage:
    python scripts/usb_dump.py
    python scripts/usb_dump.py --json
"""

import json
import platform
import re
import subprocess
import sys
from dataclasses import asdict, dataclass, field

DEVICE_NAME = "ou-audio-effects"


@dataclass
class InterfaceInfo:
    iface_num: int
    class_code: int
    subclass: int
    protocol: int
    alt_setting: int
    num_endpoints: int
    driver: str = ""


@dataclass
class DeviceInfo:
    vid: int
    pid: int
    bcd_usb: int
    bcd_device: int
    device_class: int
    device_subclass: int
    device_protocol: int
    max_packet_size0: int
    num_configs: int
    location_id: int
    manufacturer: str = ""
    product: str = ""
    serial: str = ""
    speed: str = ""
    interfaces: list[InterfaceInfo] = field(default_factory=list)


def _ioreg_device(device_name: str) -> str:
    result = subprocess.run(
        ["ioreg", "-p", "IOUSB", "-w0", "-l", "-r", "-n", device_name],
        capture_output=True, text=True, timeout=10,
    )
    return result.stdout


def _ioreg_interfaces() -> str:
    result = subprocess.run(
        ["ioreg", "-w0", "-l", "-r", "-c", "IOUSBHostInterface"],
        capture_output=True, text=True, timeout=10,
    )
    return result.stdout


def _ioreg_int(block: str, key: str, default: int = 0) -> int:
    m = re.search(rf'"{key}"\s*=\s*(-?\d+)', block)
    return int(m.group(1)) if m else default


def _ioreg_str(block: str, key: str) -> str:
    m = re.search(rf'"{key}"\s*=\s*"([^"]*)"', block)
    return m.group(1) if m else ""


def _parse_device(tree: str) -> DeviceInfo | None:
    loc_id = _ioreg_int(tree, "locationID")
    if not loc_id:
        return None

    speed_map = {1: "full (12 Mbps)", 2: "high (480 Mbps)", 3: "super (5 Gbps)"}

    return DeviceInfo(
        vid=_ioreg_int(tree, "idVendor"),
        pid=_ioreg_int(tree, "idProduct"),
        bcd_usb=_ioreg_int(tree, "bcdUSB"),
        bcd_device=_ioreg_int(tree, "bcdDevice"),
        device_class=_ioreg_int(tree, "bDeviceClass"),
        device_subclass=_ioreg_int(tree, "bDeviceSubClass"),
        device_protocol=_ioreg_int(tree, "bDeviceProtocol"),
        max_packet_size0=_ioreg_int(tree, "bMaxPacketSize0"),
        num_configs=_ioreg_int(tree, "bNumConfigurations"),
        location_id=loc_id,
        manufacturer=_ioreg_str(tree, "kUSBVendorString"),
        product=_ioreg_str(tree, "kUSBProductString"),
        serial=_ioreg_str(tree, "kUSBSerialNumberString"),
        speed=speed_map.get(_ioreg_int(tree, "USBSpeed"), "unknown"),
    )


def _parse_interfaces(tree: str, location_id: int) -> list[InterfaceInfo]:
    blocks = re.split(r"\n\+-o ", tree)
    ifaces: list[InterfaceInfo] = []

    for block in blocks:
        b_loc = re.search(r'"locationID"\s*=\s*(-?\d+)', block)
        if not b_loc or int(b_loc.group(1)) != location_id:
            continue

        driver = ""
        dm = re.search(r'"UsbExclusiveOwner"\s*=\s*"([^"]*)"', block)
        if dm and "pid" not in dm.group(1):
            driver = dm.group(1)

        iface = InterfaceInfo(
            iface_num=_ioreg_int(block, "bInterfaceNumber"),
            class_code=_ioreg_int(block, "bInterfaceClass"),
            subclass=_ioreg_int(block, "bInterfaceSubClass"),
            protocol=_ioreg_int(block, "bInterfaceProtocol"),
            alt_setting=_ioreg_int(block, "bAlternateSetting"),
            num_endpoints=_ioreg_int(block, "bNumEndpoints"),
            driver=driver,
        )
        ifaces.append(iface)

    return ifaces


_RULE = "─" * 64


def _fmt_device(info: DeviceInfo) -> list[str]:
    names = {
        0x01: "Audio",
        0x02: "CDC (Communications)",
        0x03: "HID",
        0x06: "Image",
        0x07: "Printer",
        0x08: "Mass Storage",
        0x09: "Hub",
        0x0A: "CDC Data",
        0x0B: "Smart Card",
        0x0E: "Video",
        0xDC: "Diagnostic",
        0xE0: "Wireless Controller",
        0xEF: "Miscellaneous (IAD)",
        0xFE: "Application Specific",
        0xFF: "Vendor Specific",
    }
    return names.get(cls, f"0x{cls:02X}")


def _class_name(cls: int) -> str:
    names = {
        0x01: "Audio",
        0x02: "CDC (Communications)",
        0x03: "HID",
        0x06: "Image",
        0x07: "Printer",
        0x08: "Mass Storage",
        0x09: "Hub",
        0x0A: "CDC Data",
        0x0B: "Smart Card",
        0x0E: "Video",
        0xDC: "Diagnostic",
        0xE0: "Wireless Controller",
        0xEF: "Miscellaneous (IAD)",
        0xFE: "Application Specific",
        0xFF: "Vendor Specific",
    }
    return names.get(cls, f"0x{cls:02X}")


def _subcls_name(cls: int, subcls: int) -> str:
    if cls == 0x01:
        return {1: "Audio Control", 2: "Audio Streaming", 3: "MIDI Streaming"}.get(subcls, f"0x{subcls:02X}")
    if cls == 0x02:
        return {1: "Direct Line", 2: "ACM"}.get(subcls, f"0x{subcls:02X}")
    if cls == 0xEF:
        return {1: "Interface Association", 2: "Common Class"}.get(subcls, f"0x{subcls:02X}")
    return f"0x{subcls:02X}"


def _prot_name(cls: int, subcls: int, prot: int) -> str:
    if cls == 0x02 and subcls == 0x02:
        return {0: "none", 1: "AT Commands (V.250)"}.get(prot, f"0x{prot:02X}")
    if cls == 0xEF and subcls == 0x02:
        return {1: "IAD (Interface Association Descriptor)"}.get(prot, f"0x{prot:02X}")
    if prot == 0:
        return "none"
    return f"0x{prot:02X}"


_RULE = "─" * 64


def _fmt_device(info: DeviceInfo) -> list[str]:
    usb_ver = f"{info.bcd_usb >> 8}.{info.bcd_usb & 0xFF}"
    dev_ver = f"{info.bcd_device >> 8}.{info.bcd_device & 0xFF:02d}"
    return [
        "  DEVICE DESCRIPTOR",
        f"    bcdUSB             {usb_ver}",
        f"    bDeviceClass        {info.device_class} ({_class_name(info.device_class)})",
        f"    bDeviceSubClass     {info.device_subclass} ({_subcls_name(info.device_class, info.device_subclass)})",
        f"    bDeviceProtocol     {info.device_protocol} ({_prot_name(info.device_class, info.device_subclass, info.device_protocol)})",
        f"    bMaxPacketSize0     {info.max_packet_size0}",
        f"    idVendor            0x{info.vid:04X}",
        f"    idProduct           0x{info.pid:04X}",
        f"    bcdDevice           {dev_ver}",
        f"    iManufacturer       \"{info.manufacturer}\"",
        f"    iProduct            \"{info.product}\"",
        f"    iSerialNumber       \"{info.serial}\"",
        f"    bNumConfigurations  {info.num_configs}",
        f"    Speed               {info.speed}",
    ]


def _fmt_config() -> list[str]:
    return [
        "  CONFIGURATION (index 1)",
        "    bNumInterfaces      5",
        "    bmAttributes        0xC0 (self-powered)",
        "    bMaxPower           100 mA",
        '    Description         "CLI + UAC1 Audio"',
    ]


def _fmt_interfaces(ifaces: list[InterfaceInfo]) -> list[str]:
    lines: list[str] = []
    for iface in sorted(ifaces, key=lambda i: i.iface_num):
        cn = _class_name(iface.class_code)
        sn = _subcls_name(iface.class_code, iface.subclass)
        pn = _prot_name(iface.class_code, iface.subclass, iface.protocol)

        lines.append("")
        lines.append(f"  INTERFACE {iface.iface_num}  (alt {iface.alt_setting})")
        lines.append(f"    bInterfaceClass     {iface.class_code} ({cn})")
        lines.append(f"    bInterfaceSubClass  {iface.subclass} ({sn})")
        lines.append(f"    bInterfaceProtocol  {iface.protocol} ({pn})")
        lines.append(f"    bNumEndpoints       {iface.num_endpoints}")

        if iface.driver:
            lines.append(f"    macOS driver        {iface.driver}")

        # purpose annotations
        purposes = {
            0: "CLI command (ACM control + interrupt EP)  → /dev/cu.*",
            1: "CLI data   (Bulk IN + Bulk OUT)           → /dev/cu.*",
            2: "UAC1 Audio Control (no endpoints)",
            3: "UAC1 AS-OUT (alt 0: zero-bandwidth; alt 1: 2 isoc EPs)",
            4: "UAC1 AS-IN  (alt 0: zero-bandwidth; alt 1: 1 isoc EP)",
        }
        if iface.iface_num in purposes:
            lines.append(f"    Purpose              {purposes[iface.iface_num]}")

    return lines


def _fmt_endpoints() -> list[str]:
    eps = [
        ("0x01", "OUT", "Bulk",       "64", "CLI data            (EP1)"),
        ("0x81", "IN",  "Bulk",       "64", "CLI data            (EP1)"),
        ("0x82", "IN",  "Interrupt",  "8",  "CLI cmd status      (EP2)"),
        ("0x03", "OUT", "Isoc",       "84", "AS-OUT data         (EP3)"),
        ("0x83", "IN",  "Isoc",       "84", "AS-IN data          (EP3)"),
        ("0x84", "IN",  "Isoc",       "3",  "AS-OUT feedback     (EP4, 32 ms)"),
    ]
    lines = ["  ENDPOINT SUMMARY (full-speed, max packet size)"]
    for addr, dir_, typ, mps, desc in eps:
        lines.append(f"    {addr}  {dir_:3s}  {typ:12s}  {mps:>3s}B   {desc}")
    lines.append("")
    lines.append("  SAMPLE RATE  40506 Hz  (48 MHz / 1185)")
    lines.append("  FORMAT       mono, 16-bit signed PCM, little-endian")
    return lines


def _fmt_strings(info: DeviceInfo) -> list[str]:
    return [
        "  STRING DESCRIPTORS (USB 2.0, langid = English)",
        f"    Manufacturer    \"{info.manufacturer}\"",
        f"    Product         \"{info.product}\"",
        f"    Serial          \"{info.serial}\"",
        '    Configuration   "CLI + UAC1 Audio"',
        '    Interface 0-1   "CDC CLI"',
    ]


def main() -> None:
    if platform.system() != "Darwin":
        print("Error: usb-dump requires macOS (uses ioreg).", file=sys.stderr)
        sys.exit(1)

    use_json = "--json" in sys.argv

    dev_tree = _ioreg_device(DEVICE_NAME)
    info = _parse_device(dev_tree)
    if info is None:
        print(f"Device '{DEVICE_NAME}' not found.", file=sys.stderr)
        sys.exit(1)

    iface_tree = _ioreg_interfaces()
    info.interfaces = _parse_interfaces(iface_tree, info.location_id)

    if use_json:
        device_fields = asdict(info)
        del device_fields["interfaces"]
        print(json.dumps({
            "device": device_fields,
            "interfaces": [asdict(i) for i in sorted(info.interfaces, key=lambda x: x.iface_num)],
        }, indent=2))
        return

    header = f"  USB Descriptor Dump — {info.manufacturer} {info.product}"
    sub = f"  VID:PID = 0x{info.vid:04X}:0x{info.pid:04X}  |  Serial = {info.serial}  |  Location = 0x{info.location_id:X}"

    print()
    print(header)
    print(sub)
    print(_RULE)

    for line in _fmt_device(info):
        print(line)
    print()
    for line in _fmt_config():
        print(line)

    print(_RULE)
    for line in _fmt_interfaces(info.interfaces):
        print(line)

    print()
    print(_RULE)
    for line in _fmt_endpoints():
        print(line)

    print(_RULE)
    for line in _fmt_strings(info):
        print(line)

    print()


if __name__ == "__main__":
    main()
