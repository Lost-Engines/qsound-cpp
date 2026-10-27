#!/usr/bin/env python3
"""Export editable C++ coefficient tables from an external Creative CSP or PVL.

For research and reproducibility only: normal builds use qsound/coefficients.h.
The firmware is decoded by the inspection tool, never executed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def header(banks, source_hash):
    banks = sorted(banks, key=lambda bank: bank['rate'])
    if [bank['rate'] for bank in banks] != [8000, 11025, 22050, 44100]:
        raise ValueError('expected all four QSound coefficient banks')
    positions = banks[0]['positions']
    if len(positions) != 33 or any(len(row) != 4 for row in positions):
        raise ValueError('expected 33 rows of four position gains')
    if any(bank['positions'] != positions for bank in banks):
        raise ValueError('rate banks do not share their position gains')
    lines = [
        '#pragma once', '', '#include <array>', '#include <cstddef>', '#include <cstdint>',
        '#include "spatial_tables.h"', '', 'namespace qsound::coefficients {', '',
        '// Editable numerical parameters recovered from Creative QSound firmware.',
        '// Taps are in authored oldest-to-newest order; see docs/coefficients.md.',
        f'// Source asset SHA-256: {source_hash}',
        '// Reproduce with tools/export_coefficients.py; no firmware is needed at runtime.', '',
        'template<size_t TapCount>', 'struct filter_bank {',
        '  std::array<int16_t, TapCount> left;', '  std::array<int16_t, TapCount> right;',
        '  unsigned int direct_delay_samples;', '};', '',
        'inline constexpr std::array<position_gains, spatial_position_count> positions{',
    ]
    names = ('direct_left', 'direct_right', 'filtered_left', 'filtered_right')
    for row in positions:
        if any(not -32768 <= value <= 32767 for value in row):
            raise ValueError('position gain exceeds signed 16-bit range')
        lines.append('  position_gains{')
        lines.extend(f'    .{name}{{{value}}},' for name, value in zip(names, row))
        lines.append('  },')
    lines.extend(['};', ''])
    for bank in banks:
        left, right = bank['left_filter']['taps'], bank['right_filter']['taps']
        if not left or len(left) != len(right):
            raise ValueError('expected equal nonempty left/right filters')
        if any(not -32768 <= value <= 32767 for value in left + right):
            raise ValueError('filter coefficient exceeds signed 16-bit range')
        lines.append(f"inline constexpr filter_bank<{len(left)}> bank_{bank['rate']}{{")
        for name, taps in [('left', left), ('right', right)]:
            lines.append(f'  .{name}{{')
            for offset in range(0, len(taps), 8):
                lines.append('    ' + ', '.join(map(str, taps[offset:offset + 8])) + ',')
            lines.append('  },')
        lines.extend([f"  .direct_delay_samples{{{bank['direct_delay_samples']}}},", '};', ''])
    lines.append('} // namespace qsound::coefficients')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware', type=Path)
    parser.add_argument('--tables', type=Path, default=Path('build/qsound_tables'))
    parser.add_argument('--output', type=Path, help='write here instead of standard output')
    args = parser.parse_args()
    banks = json.loads(subprocess.check_output([str(args.tables.resolve()), str(args.firmware.resolve())]))
    result = header(banks, hashlib.sha256(args.firmware.read_bytes()).hexdigest())
    if args.output:
        args.output.write_text(result, encoding='utf-8', newline='\n')
    else:
        sys.stdout.write(result)


if __name__ == '__main__':
    main()
