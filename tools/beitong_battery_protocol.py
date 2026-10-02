"""KP20D receiver status only, from the active official 20260922 frontend.

Do not use for other models, report IDs, or firmware commands.
"""

REPORT_LENGTH = 64
REPORT_ID = 0x02
STATUS_COMMAND = 0x15


def status_request():
    return bytes([REPORT_ID, STATUS_COMMAND]) + bytes(REPORT_LENGTH - 2)


def parse_status(report):
    if len(report) != REPORT_LENGTH:
        return None
    if report[0] != REPORT_ID or report[1] != STATUS_COMMAND:
        return None
    if report[2] > 100:
        return None
    return {'percentageCandidate': report[2],
            'chargeStateRaw': report[4] & 0x0F,
            'quality': 'vendor_status_needs_hardware_comparison'}
