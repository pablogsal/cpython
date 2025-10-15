"""Binary file collector for profiling data.

This module provides a collector that writes profiling samples to a binary file
format with string and frame table deduplication for efficient storage.
"""

import struct
import time
import hashlib
from typing import Dict, List, Tuple
from io import BytesIO


# Constants
BINARY_FORMAT_MAGIC = 0x50594442  # "PYDB"
BINARY_FORMAT_VERSION = 1


def encode_varint_u64(value: int) -> bytes:
    """Encode an unsigned 64-bit integer as a varint (LEB128)."""
    result = bytearray()
    while value >= 0x80:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result.append(value & 0x7F)
    return bytes(result)


def encode_varint_u32(value: int) -> bytes:
    """Encode an unsigned 32-bit integer as a varint (LEB128)."""
    return encode_varint_u64(value)


def encode_varint_i32(value: int) -> bytes:
    """Encode a signed 32-bit integer as a zigzag varint."""
    # Zigzag encoding: map signed to unsigned
    # -1 -> 1, -2 -> 3, 0 -> 0, 1 -> 2, 2 -> 4, etc.
    zigzag = (value << 1) ^ (value >> 31)
    return encode_varint_u32(zigzag & 0xFFFFFFFF)


class BinaryCollector:
    """Collector that writes profiling samples to a binary file.

    The binary format uses string and frame tables for deduplication,
    similar to the Gecko format, resulting in significantly smaller files.

    Format structure:
        [Header - 64 bytes]
        [Thread Data - variable size]
          - ThreadHeader (16 bytes per thread)
          - Samples (varint-encoded)
        [String Table - variable size]
        [Frame Table - variable size]
        [Footer - 32 bytes]
    """

    def __init__(self, filename: str, sample_interval_usec: int = 100, skip_idle: bool = False):
        """Initialize the binary collector.

        Args:
            filename: Path to the binary file to write
            sample_interval_usec: Sampling interval in microseconds
            skip_idle: Whether to skip idle threads (not used in binary mode)
        """
        self.filename = filename
        self.sample_interval_usec = sample_interval_usec
        self.skip_idle = skip_idle

        # Start time
        self.start_time_us = int(time.time() * 1_000_000)

        # String table: maps string -> index
        self.string_table: Dict[str, int] = {}
        self.strings: List[str] = []

        # Frame table: maps (filename, funcname, lineno) -> index
        self.frame_table: Dict[Tuple[int, int, int], int] = {}
        self.frames: List[Tuple[int, int, int]] = []  # (filename_idx, funcname_idx, lineno)

        # Open file immediately and write placeholder header
        self.file = open(filename, 'wb')
        self.file.write(b'\x00' * 64)  # Placeholder header

        # Track previous timestamp for delta encoding (per thread)
        self.thread_prev_timestamp: Dict[int, int] = {}

        # Total samples
        self.total_samples = 0
        self.last_progress_print = 0

    def _intern_string(self, s: str) -> int:
        """Add a string to the string table and return its index."""
        if s in self.string_table:
            return self.string_table[s]
        idx = len(self.strings)
        self.strings.append(s)
        self.string_table[s] = idx
        return idx

    def _intern_frame(self, filename: str, funcname: str, lineno: int) -> int:
        """Add a frame to the frame table and return its index."""
        filename_idx = self._intern_string(filename)
        funcname_idx = self._intern_string(funcname)

        key = (filename_idx, funcname_idx, lineno)
        if key in self.frame_table:
            return self.frame_table[key]

        idx = len(self.frames)
        self.frames.append(key)
        self.frame_table[key] = idx
        return idx

    def collect(self, stack_frames):
        """Collect a sample from the profiler.

        Args:
            stack_frames: List of InterpreterFrame objects from RemoteUnwinder
        """
        if not stack_frames:
            return

        timestamp_us = int(time.time() * 1_000_000)

        for interpreter_frame in stack_frames:
            interpreter_id = interpreter_frame.interpreter_id

            for thread in interpreter_frame.threads:
                thread_id = thread.thread_id
                status = thread.status

                # Initialize thread timestamp if needed
                if thread_id not in self.thread_prev_timestamp:
                    self.thread_prev_timestamp[thread_id] = self.start_time_us

                # Convert frames to indices
                frame_indices = []
                for frame_info in thread.frame_info:
                    frame_idx = self._intern_frame(
                        frame_info.filename,
                        frame_info.funcname,
                        frame_info.lineno
                    )
                    frame_indices.append(frame_idx)

                # Write sample directly to disk (no thread header, just raw data)
                # Write thread_id (8 bytes)
                self.file.write(struct.pack('<Q', thread_id))

                # Write interpreter_id (4 bytes)
                self.file.write(struct.pack('<I', interpreter_id))

                # Write timestamp delta
                delta = timestamp_us - self.thread_prev_timestamp[thread_id]
                self.file.write(encode_varint_u64(delta))
                self.thread_prev_timestamp[thread_id] = timestamp_us

                # Write status (1 byte)
                self.file.write(struct.pack('<B', status))

                # Write stack depth
                self.file.write(encode_varint_u32(len(frame_indices)))

                # Write frame indices
                for frame_idx in frame_indices:
                    self.file.write(encode_varint_u32(frame_idx))

                self.total_samples += 1

    def export(self, filename: str = None):
        """Finalize the binary file by writing tables and updating header.

        Args:
            filename: Ignored (file was opened in __init__)
        """
        if self.total_samples > 0:
            print()  # Newline after collection progress

        print(f"Finalizing binary file: {self.filename}...")

        # Get where sample data ends (before tables)
        sample_data_end = self.file.tell()

        # Write string table
        print(f"Writing string table: {len(self.strings)} strings...", end='', flush=True)
        string_table_offset = self.file.tell()
        for s in self.strings:
            s_bytes = s.encode('utf-8')
            self.file.write(encode_varint_u32(len(s_bytes)))
            self.file.write(s_bytes)
        print(" done")

        # Write frame table
        print(f"Writing frame table: {len(self.frames)} frames...", end='', flush=True)
        frame_table_offset = self.file.tell()
        for filename_idx, funcname_idx, lineno in self.frames:
            self.file.write(encode_varint_u32(filename_idx))
            self.file.write(encode_varint_u32(funcname_idx))
            self.file.write(encode_varint_i32(lineno))
        print(" done")

        # Write footer
        footer_offset = self.file.tell()
        file_size = footer_offset + 32
        checksum = hashlib.md5(b'').digest()  # Placeholder checksum

        self.file.write(struct.pack('<II', len(self.strings), len(self.frames)))
        self.file.write(struct.pack('<Q', file_size))
        self.file.write(checksum)

        # Update header with actual values
        self.file.seek(0)
        self.file.write(struct.pack('<II', BINARY_FORMAT_MAGIC, BINARY_FORMAT_VERSION))
        self.file.write(struct.pack('<QQ', self.start_time_us, self.sample_interval_usec))
        self.file.write(struct.pack('<II', self.total_samples, len(self.thread_prev_timestamp)))  # thread count
        self.file.write(struct.pack('<QQ', string_table_offset, frame_table_offset))
        self.file.write(b'\x00' * 16)  # Reserved

        self.file.close()
