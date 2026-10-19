#!/usr/bin/env python3
"""Check the explicit model's arithmetic against independent scalar equations.

This checks implementation of four stated hypotheses. It cannot establish that
any of them reproduces ST18932 arithmetic or original QSound audio.
"""
import argparse
import json
from pathlib import Path
import random
import struct
import subprocess


def reference(inputs, table, positions, rounding, accumulation):
    def clip_acc(value):
        return min(2147483647, max(-2147483648, value))

    def add(accumulator, product):
        value = accumulator + product
        return clip_acc(value) if accumulation == 'each' else value

    def word(value):
        value = clip_acc(value)
        if rounding == 'nearest':
            value += 32768
        return min(32767, max(-32768, value // 65536))

    mixed = [[], [], [], []]
    gains = [table['positions'][position] for position in positions]
    result = []
    delay = table['direct_delay_samples']
    for frame, source in enumerate(inputs):
        for lane in range(4):
            mixed[lane].append(word(add(add(0, source[0] * gains[0][lane] * 2), source[1] * gains[1][lane] * 2)))
        outputs = []
        for channel, name in enumerate(('left_filter', 'right_filter')):
            accumulator = mixed[channel][frame - delay] * 65536 if frame >= delay else 0
            taps = table[name]['taps']
            for tap, coefficient in enumerate(taps):
                at = frame - len(taps) + 1 + tap
                value = mixed[channel + 2][at] if at >= 0 else 0
                accumulator = add(accumulator, value * coefficient * 2)
            outputs.append(word(accumulator))
        result.extend(outputs)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('firmware', type=Path)
    parser.add_argument('--probe', type=Path, default=Path('build/qsound_model_probe'))
    parser.add_argument('--tables', type=Path, default=Path('build/qsound_tables'))
    args = parser.parse_args()
    tables = json.loads(subprocess.check_output([str(args.tables.resolve()), str(args.firmware.resolve())]))
    randomizer = random.Random(0x53504154)
    inputs = [[0, 0]] * 4 + [[32767, 0], [0, -32768]]
    inputs += [[randomizer.randrange(-32768, 32768), randomizer.randrange(-32768, 32768)] for unused in range(256)]
    inputs += [[32767, 32767]] * 128 + [[-32768, -32768]] * 128 + [[0, 0]] * 64
    pcm = struct.pack('<' + 'h' * (len(inputs) * 2), *(sample for frame in inputs for sample in frame))
    cases = 0
    for table in tables:
        for positions in ((0, 32), (16, 16), (32, 0)):
            for rounding in ('floor', 'nearest'):
                for accumulation in ('each', 'output'):
                    expected = reference(inputs, table, positions, rounding, accumulation)
                    result = subprocess.check_output([str(args.probe.resolve()), str(args.firmware.resolve()),
                        str(table['rate']), *map(str, positions), rounding, accumulation], input=pcm)
                    actual = list(struct.unpack('<' + 'h' * (len(result) // 2), result))
                    assert actual == expected, (table['rate'], positions, rounding, accumulation)
                    cases += 1
    command = [str(args.probe.resolve()), str(args.firmware.resolve()), '8000', '16', '16', 'floor', 'each']
    process = subprocess.run(command, input=b'\0', capture_output=True)
    assert process.returncode != 0 and b'incomplete PCM frame' in process.stderr
    command[3] = '33'
    process = subprocess.run(command, input=pcm, capture_output=True)
    assert process.returncode != 0 and b'position 33 remains unresolved' in process.stderr
    print(f'{cases} arithmetic-model cases / {cases * len(inputs)} stereo frames match independent scalar equations; malformed PCM and unresolved position rejected. This is not original-DSP verification.')


if __name__ == '__main__':
    main()
