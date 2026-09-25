#!/usr/bin/env python3
"""Convert a mono 48 kHz PCM16 WAV into a firmware C audio asset."""

import argparse
import wave
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("symbol")
    args = parser.parse_args()

    with wave.open(str(args.input), "rb") as wav_file:
        if wav_file.getnchannels() != 1:
            raise ValueError("input WAV must be mono")
        if wav_file.getsampwidth() != 2:
            raise ValueError("input WAV must be 16-bit PCM")
        if wav_file.getframerate() != 48000:
            raise ValueError("input WAV must be 48 kHz")
        sample_count = wav_file.getnframes()
        pcm = wav_file.readframes(sample_count)

    samples = [int.from_bytes(pcm[index:index + 2], "little", signed=True)
               for index in range(0, len(pcm), 2)]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    header_name = args.output.with_suffix(".h").name
    with args.output.open("w", encoding="ascii", newline="\n") as source:
        source.write('#include "{}"\n\n'.format(header_name))
        source.write("const int16_t {}[] = {{\n".format(args.symbol))
        for index in range(0, len(samples), 12):
            source.write("    {},\n".format(", ".join(str(value) for value in samples[index:index + 12])))
        source.write("};\n")
        source.write("const size_t {}_count = {}U;\n".format(args.symbol, len(samples)))

    header = args.output.with_suffix(".h")
    guard = "{}_H".format(args.symbol.upper())
    with header.open("w", encoding="ascii", newline="\n") as header_file:
        header_file.write("#ifndef {}\n#define {}\n\n".format(guard, guard))
        header_file.write("#include <stddef.h>\n#include <stdint.h>\n\n")
        header_file.write("#define {}_SAMPLE_RATE_HZ 48000U\n".format(args.symbol.upper()))
        header_file.write("extern const int16_t {}[];\n".format(args.symbol))
        header_file.write("extern const size_t {}_count;\n\n".format(args.symbol))
        header_file.write("#endif\n")


if __name__ == "__main__":
    main()
