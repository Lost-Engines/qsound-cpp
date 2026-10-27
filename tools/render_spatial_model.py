#!/usr/bin/env python3
"""Render four explicit arithmetic hypotheses for the recovered spatial path.

Outputs maximum-compression FLACs plus a comparison manifest. These are
analytical-model auditions, not recordings or verified original QSound output.
An external original firmware file, model probe and FLAC encoder are required.
"""
import argparse
from array import array
import hashlib
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import wave


def read_pcm(path):
    with wave.open(str(path), 'rb') as source:
        channels, width, rate = source.getnchannels(), source.getsampwidth(), source.getframerate()
        if source.getcomptype() != 'NONE' or channels not in (1, 2) or width not in (1, 2):
            raise ValueError('input must be uncompressed mono/stereo 8-bit or 16-bit PCM WAV')
        data = source.readframes(source.getnframes())
    if width == 1:
        values = [(value - 128) * 256 for value in data]
    else:
        values = list(struct.unpack('<' + 'h' * (len(data) // 2), data))
    if channels == 1:
        values = [value for sample in values for value in (sample, 0)]
    return rate, len(values) // 2, struct.pack('<' + 'h' * len(values), *values)


def wav_bytes(pcm, rate):
    target = io.BytesIO()
    with wave.open(target, 'wb') as destination:
        destination.setnchannels(2)
        destination.setsampwidth(2)
        destination.setframerate(rate)
        destination.writeframes(pcm)
    return target.getvalue()


def signed_samples(pcm):
    result = array('h')
    result.frombytes(pcm)
    if sys.byteorder != 'little':
        result.byteswap()
    return result


def default_tool(name):
    """Find single-config or Release multi-config builds in the usual directory."""
    suffix = '.exe' if sys.platform == 'win32' else ''
    candidates = [Path('build') / (name + suffix), Path('build/Release') / (name + suffix)]
    return next((path for path in candidates if path.is_file()), candidates[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--firmware', type=Path, default=Path('firmware/WFM0001A.CSP'))
    parser.add_argument('--firmware-rate', type=int, choices=(8000, 11025, 22050, 44100), default=8000,
                        help='bank selection, independently of the input playback rate; TFX chooses 8000')
    parser.add_argument('--position', type=int, choices=range(33), default=16)
    parser.add_argument('--right-position', type=int, choices=range(33), default=16)
    parser.add_argument('--probe', type=Path, default=default_tool('qsound_model_probe'))
    parser.add_argument('--tables', type=Path, default=default_tool('qsound_tables'))
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f'input WAV not found: {args.input}')
    if not args.firmware.is_file():
        parser.error(f'firmware not found: {args.firmware}; run python3 tools/fetch_firmware.py or pass --firmware')
    for label, path in (('probe', args.probe), ('tables', args.tables)):
        if not path.is_file():
            parser.error(f'{label} executable not found: {path}; build the tools or pass --{label}')
    encoder = shutil.which('flac')
    if not encoder:
        parser.error('the flac command-line encoder is required; install it and make it available on PATH')
    rate, frames, pcm = read_pcm(args.input)
    tables = json.loads(subprocess.check_output([str(args.tables.resolve()), str(args.firmware.resolve())]))
    selected = next(row for row in tables if row['rate'] == args.firmware_rate)
    tail = max(len(selected['left_filter']['taps']), len(selected['right_filter']['taps']), selected['direct_delay_samples']) + 1
    pcm += bytes(tail * 4)
    args.output.mkdir(parents=True, exist_ok=True)
    baseline = None
    renders = []
    for rounding in ('floor', 'nearest'):
        for accumulation in ('each', 'output'):
            result = subprocess.check_output([str(args.probe.resolve()), str(args.firmware.resolve()),
                str(args.firmware_rate), str(args.position), str(args.right_position), rounding, accumulation], input=pcm)
            if len(result) != len(pcm):
                raise ValueError('model output does not match the input frame count')
            decoded = signed_samples(result)
            if baseline is None:
                baseline = decoded
            differences = [abs(value - reference) for value, reference in zip(decoded, baseline)]
            name = f'{args.input.stem}-p{args.position}-{rounding}-{accumulation}.flac'
            subprocess.run([encoder, '--best', '--silent', '--force', '--output-name', str(args.output / name), '-'],
                           input=wav_bytes(result, rate), check=True)
            renders.append(dict(file=name, rounding=rounding, accumulation=accumulation,
                                pcm_sha256=hashlib.sha256(result).hexdigest(),
                                maximum_absolute_sample_difference=max(differences, default=0),
                                different_samples=sum(value != 0 for value in differences),
                                rail_samples=sum(value in (-32768, 32767) for value in decoded)))
    manifest = dict(scope='Analytical spatial-path model, not verified ST18932 audio; static source positions.',
                    input=str(args.input.resolve()), input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
                    firmware_sha256=hashlib.sha256(args.firmware.read_bytes()).hexdigest(),
                    playback_rate=rate, input_frames=frames, tail_frames=tail,
                    firmware_rate=args.firmware_rate, left_position=args.position,
                    right_position=args.right_position, renders=renders)
    (args.output / 'model-renders.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for render in renders:
        print(f"{render['file']}: {render['different_samples']} samples differ from floor/each, maximum {render['maximum_absolute_sample_difference']} LSB; {render['rail_samples']} rail samples")
    print('Analytical-model auditions only; no original-hardware or DSP-output equivalence claimed.')


if __name__ == '__main__':
    main()
