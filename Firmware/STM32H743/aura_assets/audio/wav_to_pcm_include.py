from __future__ import annotations

import argparse
import struct
import wave
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_wav", type=Path)
    parser.add_argument("output_inc", type=Path)
    args = parser.parse_args()

    with wave.open(str(args.input_wav), "rb") as wav_file:
        if wav_file.getnchannels() != 1:
            raise ValueError("Expected mono WAV")
        if wav_file.getsampwidth() != 2:
            raise ValueError("Expected 16-bit WAV")
        sample_rate = wav_file.getframerate()
        frames = wav_file.readframes(wav_file.getnframes())

    samples = struct.unpack(f"<{len(frames) // 2}h", frames)
    if sample_rate != 16000:
        source = samples
        output_count = (len(source) * 16000) // sample_rate
        converted: list[int] = []
        for index in range(output_count):
            source_position = index * sample_rate
            source_index = source_position // 16000
            fraction = source_position % 16000
            next_index = min(source_index + 1, len(source) - 1)
            interpolated = (
                source[source_index] * (16000 - fraction)
                + source[next_index] * fraction
            ) // 16000
            converted.append(interpolated)
        samples = tuple(converted)
    lines = ["/* Generated from startup_voice.wav; 16 kHz, mono, signed PCM16. */"]
    for offset in range(0, len(samples), 12):
        values = ", ".join(str(value) for value in samples[offset : offset + 12])
        lines.append(f"    {values},")
    args.output_inc.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"samples={len(samples)} duration={len(samples) / 16000:.3f}s")


if __name__ == "__main__":
    main()
