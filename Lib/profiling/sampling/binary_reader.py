"""Binary file reader for profiling data.

This module provides functionality to read profiling samples from binary files
created by BinaryCollector and replay them through other collectors.
"""

import struct
import time
from typing import List, Iterator, NamedTuple


# Constants
BINARY_FORMAT_MAGIC = 0x50594442  # "PYDB"
BINARY_FORMAT_VERSION = 1


class FrameInfo(NamedTuple):
    """Frame information matching _remote_debugging.FrameInfo structure."""
    filename: str
    lineno: int
    funcname: str


class ThreadInfo(NamedTuple):
    """Thread information matching _remote_debugging.ThreadInfo structure."""
    thread_id: int
    status: int
    frame_info: List[FrameInfo]


class InterpreterInfo(NamedTuple):
    """Interpreter information matching _remote_debugging.InterpreterInfo structure."""
    interpreter_id: int
    threads: List[ThreadInfo]


class BinaryFormatError(Exception):
    """Exception raised for binary format errors."""
    pass


def decode_varint_u64(data: bytes, offset: int = 0) -> tuple[int, int]:
    """Decode an unsigned 64-bit varint from bytes.

    Args:
        data: Bytes to decode from
        offset: Starting offset in data

    Returns:
        Tuple of (decoded_value, bytes_consumed)
    """
    # Fast path: most varints are 1-2 bytes
    byte = data[offset]
    if (byte & 0x80) == 0:
        return byte, 1

    result = byte & 0x7F
    byte = data[offset + 1]
    if (byte & 0x80) == 0:
        return result | (byte << 7), 2

    result |= (byte & 0x7F) << 7
    byte = data[offset + 2]
    if (byte & 0x80) == 0:
        return result | (byte << 14), 3

    result |= (byte & 0x7F) << 14
    byte = data[offset + 3]
    if (byte & 0x80) == 0:
        return result | (byte << 21), 4

    result |= (byte & 0x7F) << 21
    byte = data[offset + 4]
    if (byte & 0x80) == 0:
        return result | (byte << 28), 5

    # Slow path for 6+ bytes (rare)
    result |= (byte & 0x7F) << 28
    pos = offset + 5
    shift = 35
    while pos < len(data):
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if (byte & 0x80) == 0:
            return result, pos - offset
        shift += 7
        if shift >= 64:
            raise BinaryFormatError("Varint overflow")
    raise BinaryFormatError("Incomplete varint")


def decode_varint_u32(data: bytes, offset: int = 0) -> tuple[int, int]:
    """Decode an unsigned 32-bit varint."""
    value, consumed = decode_varint_u64(data, offset)
    if value > 0xFFFFFFFF:
        raise BinaryFormatError(f"Varint value {value} exceeds 32-bit range")
    return value, consumed


def decode_varint_i32(data: bytes, offset: int = 0) -> tuple[int, int]:
    """Decode a signed 32-bit zigzag varint."""
    zigzag, consumed = decode_varint_u32(data, offset)
    # Zigzag decoding: reverse the mapping
    value = (zigzag >> 1) ^ -(zigzag & 1)
    return value, consumed


class BinaryReader:
    """Reader for binary profiling data files.

    This class reads profiling data from binary files and can replay
    samples through collectors as if they were coming from a live profiler.
    """

    def __init__(self, filename: str):
        """Initialize the binary reader.

        Args:
            filename: Path to the binary file to read
        """
        self.filename = filename
        self.file = None
        self.header = None
        self.footer = None
        self.strings = []
        self.frames = []
        self.threads = []

    def __enter__(self):
        """Open the file and load metadata."""
        self.file = open(self.filename, 'rb')
        self._load_metadata()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Close the file."""
        if self.file:
            self.file.close()
            self.file = None
        return False

    def _load_metadata(self):
        """Load header, footer, and tables from the file."""
        # Read header (64 bytes)
        header_data = self.file.read(64)
        if len(header_data) != 64:
            raise BinaryFormatError("Failed to read header")

        magic, version = struct.unpack_from('<II', header_data, 0)
        if magic != BINARY_FORMAT_MAGIC:
            raise BinaryFormatError(f"Invalid magic number: 0x{magic:08x}")
        if version != BINARY_FORMAT_VERSION:
            raise BinaryFormatError(f"Unsupported version: {version}")

        start_time_us, sample_interval_us = struct.unpack_from('<QQ', header_data, 8)
        sample_count, thread_count = struct.unpack_from('<II', header_data, 24)
        string_table_offset, frame_table_offset = struct.unpack_from('<QQ', header_data, 32)

        self.header = {
            'magic': magic,
            'version': version,
            'start_time_us': start_time_us,
            'sample_interval_us': sample_interval_us,
            'sample_count': sample_count,
            'thread_count': thread_count,
            'string_table_offset': string_table_offset,
            'frame_table_offset': frame_table_offset,
        }

        # Read footer (32 bytes from end)
        self.file.seek(-32, 2)
        footer_data = self.file.read(32)
        if len(footer_data) != 32:
            raise BinaryFormatError("Failed to read footer")

        string_count, frame_count, file_size = struct.unpack_from('<IIQ', footer_data, 0)

        self.footer = {
            'string_count': string_count,
            'frame_count': frame_count,
            'file_size': file_size,
        }

        # Validate file size
        actual_size = self.file.seek(0, 2)
        if actual_size != file_size:
            raise BinaryFormatError(f"File size mismatch: {actual_size} != {file_size}")

        # Load string table
        self._load_string_table()

        # Load frame table
        self._load_frame_table()

        # Load thread metadata
        self._load_thread_metadata()

    def _load_string_table(self):
        """Load the string table from the file."""
        self.file.seek(self.header['string_table_offset'])

        # Read entire string table section at once
        string_table_size = self.header['frame_table_offset'] - self.header['string_table_offset']
        string_table_data = self.file.read(string_table_size)

        self.strings = []
        offset = 0

        for _ in range(self.footer['string_count']):
            # Read string length (varint)
            str_len, consumed = decode_varint_u32(string_table_data, offset)
            offset += consumed

            # Read string data
            str_data = string_table_data[offset:offset + str_len]
            if len(str_data) != str_len:
                raise BinaryFormatError("Failed to read string data")
            offset += str_len

            self.strings.append(str_data.decode('utf-8'))

    def _load_frame_table(self):
        """Load the frame table from the file."""
        self.file.seek(self.header['frame_table_offset'])

        # Read entire frame table section at once
        footer_offset = self.footer['file_size'] - 32
        frame_table_size = footer_offset - self.header['frame_table_offset']
        frame_table_data = self.file.read(frame_table_size)

        self.frames = []
        offset = 0

        for _ in range(self.footer['frame_count']):
            # Read filename index (varint)
            filename_idx, consumed = decode_varint_u32(frame_table_data, offset)
            offset += consumed

            # Read funcname index (varint)
            funcname_idx, consumed = decode_varint_u32(frame_table_data, offset)
            offset += consumed

            # Read lineno (signed varint)
            lineno, consumed = decode_varint_i32(frame_table_data, offset)
            offset += consumed

            self.frames.append((filename_idx, funcname_idx, lineno))

    def _load_thread_metadata(self):
        """No per-thread metadata needed - samples are just sequential."""
        # Just record the sample data region
        self.sample_data_start = 64  # After header
        self.sample_data_end = self.header['string_table_offset']
        self.sample_data_size = self.sample_data_end - self.sample_data_start

    def replay_samples(self, realtime: bool = False) -> Iterator[List[InterpreterInfo]]:
        """Replay samples from the binary file.

        Yields samples in the same format as RemoteUnwinder.get_stack_trace() would return.

        Args:
            realtime: If True, replay with actual timing delays

        Yields:
            List of InterpreterInfo objects for each sample
        """
        # Read all sample data into memory
        self.file.seek(self.sample_data_start)
        data = self.file.read(self.sample_data_size)

        # Pre-fetch lists to avoid attribute lookups in loop
        strings = self.strings
        frames = self.frames

        # Track previous timestamps per thread for delta decoding
        thread_prev_timestamp = {}

        offset = 0
        last_time = time.time() if realtime else None

        # Read samples sequentially until we reach the end
        for sample_idx in range(self.header['sample_count']):
            # Read thread_id (8 bytes)
            thread_id = struct.unpack('<Q', data[offset:offset+8])[0]
            offset += 8

            # Read interpreter_id (4 bytes)
            interp_id = struct.unpack('<I', data[offset:offset+4])[0]
            offset += 4

            # Initialize previous timestamp for this thread if needed
            if thread_id not in thread_prev_timestamp:
                thread_prev_timestamp[thread_id] = self.header['start_time_us']

            # Decode timestamp delta
            delta, consumed = decode_varint_u64(data, offset)
            offset += consumed
            timestamp_us = thread_prev_timestamp[thread_id] + delta
            thread_prev_timestamp[thread_id] = timestamp_us

            # Read status
            status = data[offset]
            offset += 1

            # Read stack depth
            stack_depth, consumed = decode_varint_u32(data, offset)
            offset += consumed

            # Build frame_info list
            frame_info = []
            for _ in range(stack_depth):
                frame_idx, consumed = decode_varint_u32(data, offset)
                offset += consumed

                filename_idx, funcname_idx, lineno = frames[frame_idx]
                frame_info.append(FrameInfo(strings[filename_idx], lineno, strings[funcname_idx]))

            # Build result - single sample with one thread
            sample_data = [InterpreterInfo(interp_id, [ThreadInfo(thread_id, status, frame_info)])]

            # Handle realtime replay timing
            if realtime and last_time and self.header['sample_interval_us'] > 0:
                interval_sec = self.header['sample_interval_us'] / 1_000_000
                elapsed = time.time() - last_time
                if elapsed < interval_sec:
                    time.sleep(interval_sec - elapsed)
                last_time = time.time()

            yield sample_data

    def _iter_thread_samples(self, thread_meta):
        """Iterate over samples for a specific thread - read on-demand into buffer."""
        # Read this thread's sample data into a buffer now (not during load)
        self.file.seek(thread_meta['file_offset'])
        data = self.file.read(thread_meta['data_size'])

        # Parse from the buffer
        offset = 0
        prev_timestamp = self.header['start_time_us']

        for _ in range(thread_meta['sample_count']):
            # Read timestamp delta
            delta, consumed = decode_varint_u64(data, offset)
            offset += consumed
            timestamp_us = prev_timestamp + delta
            prev_timestamp = timestamp_us

            # Read status (1 byte)
            status = data[offset]
            offset += 1

            # Read stack depth
            stack_depth, consumed = decode_varint_u32(data, offset)
            offset += consumed

            # Read frame indices
            frame_indices = []
            for _ in range(stack_depth):
                frame_idx, consumed = decode_varint_u32(data, offset)
                offset += consumed
                frame_indices.append(frame_idx)

            yield (timestamp_us, status, frame_indices)

    def get_info(self) -> dict:
        """Get information about the binary file.

        Returns:
            Dictionary containing file metadata
        """
        return {
            'version': self.header['version'],
            'start_time_us': self.header['start_time_us'],
            'sample_interval_us': self.header['sample_interval_us'],
            'sample_count': self.header['sample_count'],
            'thread_count': self.header['thread_count'],
            'string_count': self.footer['string_count'],
            'frame_count': self.footer['frame_count'],
            'file_size': self.footer['file_size'],
        }
