"""Gecko Profile format.

Provides dataclasses and builder for generating Gecko profile format
"""

import platform
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Any

from Lib.profiling.sampling.string_table import StringTable

PYTHON_CATEGORY = 0
OTHER_CATEGORY = 1
NATIVE_IMPLEMENTATION = None
GECKO_VERSION = 28

GECKO_CATEGORIES = [
    {"name": "Python", "color": "blue", "subcategories": ["Other"]},
    {"name": "Other", "color": "grey", "subcategories": ["Other"]},
]

@dataclass
class GeckoFrameTableSchema:
    location: int = 0
    relevantForJS: int = 1
    innerWindowID: int = 2
    implementation: int = 3
    optimizations: int = 4
    line: int = 5
    column: int = 6
    category: int = 7
    subcategory: int = 8

    def to_dict(self) -> Dict[str, int]:
        return {
            "location": self.location,
            "relevantForJS": self.relevantForJS,
            "innerWindowID": self.innerWindowID,
            "implementation": self.implementation,
            "optimizations": self.optimizations,
            "line": self.line,
            "column": self.column,
            "category": self.category,
            "subcategory": self.subcategory
        }

@dataclass
class GeckoStackTableSchema:
    prefix: int = 0
    frame: int = 1

    def to_dict(self) -> Dict[str, int]:
        return {
            "prefix": self.prefix,
            "frame": self.frame
        }

@dataclass
class GeckoSampleSchema:
    stack: int = 0
    time: int = 1
    eventDelay: int = 2

    def to_dict(self) -> Dict[str, int]:
        return {
            "stack": self.stack,
            "time": self.time,
            "eventDelay": self.eventDelay
        }

@dataclass
class GeckoMarkersSchema:
    name: int = 0
    startTime: int = 1
    endTime: int = 2
    phase: int = 3
    category: int = 4
    data: int = 5

    def to_dict(self) -> Dict[str, int]:
        return {
            "name": self.name,
            "startTime": self.startTime,
            "endTime": self.endTime,
            "phase": self.phase,
            "category": self.category,
            "data": self.data
        }

@dataclass
class GeckoFrame:
    location_id: int
    relevant_for_js: bool = False
    inner_window_id: int = 0
    implementation: Optional[str] = None
    optimizations: Optional[str] = None
    line: Optional[int] = None
    column: Optional[int] = None
    category: int = 0
    subcategory: int = 0

    def to_array(self) -> List[Any]:
        return [
            self.location_id,
            self.relevant_for_js,
            self.inner_window_id,
            self.implementation,
            self.optimizations,
            self.line,
            self.column,
            self.category,
            self.subcategory
        ]

@dataclass
class GeckoStack:
    prefix_id: Optional[int]
    frame_id: int

    def to_array(self) -> List[Any]:
        return [self.prefix_id, self.frame_id]

@dataclass
class GeckoSample:
    stack_id: Optional[int]
    time_ms: float
    event_delay: float = 0.0

    def to_array(self) -> List[Any]:
        return [self.stack_id, self.time_ms, self.event_delay]

@dataclass
class GeckoFrameTable:
    schema: GeckoFrameTableSchema = field(default_factory=GeckoFrameTableSchema)
    data: List[List[Any]] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": self.schema.to_dict(),
            "data": self.data
        }

@dataclass
class GeckoStackTable:
    schema: GeckoStackTableSchema = field(default_factory=GeckoStackTableSchema)
    data: List[List[Any]] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": self.schema.to_dict(),
            "data": self.data
        }

@dataclass
class GeckoSamples:
    schema: GeckoSampleSchema = field(default_factory=GeckoSampleSchema)
    data: List[List[Any]] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": self.schema.to_dict(),
            "data": self.data
        }

@dataclass
class GeckoMarkers:
    schema: GeckoMarkersSchema = field(default_factory=GeckoMarkersSchema)
    data: List[List[Any]] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "schema": self.schema.to_dict(),
            "data": self.data
        }

@dataclass
class GeckoThread:
    name: str
    isMainThread: bool = False
    processType: str = "default"
    processName: str = "Python"
    processStartupTime: float = 0.0
    processShutdownTime: Optional[float] = None
    registerTime: int = 0
    unregisterTime: Optional[int] = None
    pausedRanges: List[Any] = field(default_factory=list)
    pid: int = 0
    tid: int = 0
    stringTable: List[str] = field(default_factory=list)
    frameTable: GeckoFrameTable = field(default_factory=GeckoFrameTable)
    stackTable: GeckoStackTable = field(default_factory=GeckoStackTable)
    samples: GeckoSamples = field(default_factory=GeckoSamples)
    markers: GeckoMarkers = field(default_factory=GeckoMarkers)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "isMainThread": self.isMainThread,
            "processType": self.processType,
            "processName": self.processName,
            "processStartupTime": self.processStartupTime,
            "processShutdownTime": self.processShutdownTime,
            "registerTime": self.registerTime,
            "unregisterTime": self.unregisterTime,
            "pausedRanges": self.pausedRanges,
            "pid": self.pid,
            "tid": self.tid,
            "stringTable": self.stringTable,
            "frameTable": self.frameTable.to_dict(),
            "stackTable": self.stackTable.to_dict(),
            "samples": self.samples.to_dict(),
            "markers": self.markers.to_dict()
        }

@dataclass
class GeckoMeta:
    version: int = GECKO_VERSION
    startTime: float = 0.0
    shutdownTime: Optional[float] = None
    categories: List[Dict[str, Any]] = field(default_factory=lambda: GECKO_CATEGORIES)
    markerSchema: List[Any] = field(default_factory=list)
    interval: int = 1
    stackwalk: int = 1
    debug: int = 0
    gcpoison: int = 0
    processType: int = 0
    presymbolicated: bool = True
    platform: str = field(default_factory=platform.system)
    oscpu: str = field(default_factory=lambda: f"{platform.machine()} {platform.system()} {platform.release()}")
    misc: str = field(default_factory=lambda: f"Python {platform.python_version()}")

    def to_dict(self) -> Dict[str, Any]:
        return {
            "version": self.version,
            "startTime": self.startTime,
            "shutdownTime": self.shutdownTime,
            "categories": self.categories,
            "markerSchema": self.markerSchema,
            "interval": self.interval,
            "stackwalk": self.stackwalk,
            "debug": self.debug,
            "gcpoison": self.gcpoison,
            "processType": self.processType,
            "presymbolicated": self.presymbolicated,
            "platform": self.platform,
            "oscpu": self.oscpu,
            "misc": self.misc,
        }

@dataclass
class GeckoProfile:
    meta: Dict[str, Any]
    libs: List[Any] = field(default_factory=list)
    pages: List[Any] = field(default_factory=list)
    pausedRanges: List[Any] = field(default_factory=list)
    threads: List[Dict[str, Any]] = field(default_factory=list)
    processes: List[Any] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "meta": self.meta,
            "libs": self.libs,
            "pages": self.pages,
            "pausedRanges": self.pausedRanges,
            "threads": self.threads,
            "processes": self.processes
        }

class GeckoBuilder:
    def __init__(self, string_table: StringTable, start_time: float = None):
        self.string_table = string_table
        self.start_time = start_time
        self.threads_data = {}

    def add_sample(self, frames_list, timestamp, thread_id=0):
        if thread_id not in self.threads_data:
            self.threads_data[thread_id] = {
                "frames": [],
                "frame_map": {},
                "stacks": [],
                "stack_map": {},
                "samples": []
            }

        thread_data = self.threads_data[thread_id]

        frame_ids = []
        for frame in frames_list:
            if frame not in thread_data["frame_map"]:
                frame_index = len(thread_data["frames"])
                thread_data["frame_map"][frame] = frame_index

                filename, lineno, funcname = frame
                location_id = self.string_table.intern(funcname)

                gecko_frame = GeckoFrame(
                    location_id=location_id,
                    relevant_for_js=False,
                    inner_window_id=0,
                    implementation=None,
                    optimizations=None,
                    line=lineno,
                    column=None,
                    category=self._get_frame_category(frame),
                    subcategory=0
                )

                thread_data["frames"].append(gecko_frame)

            frame_ids.append(thread_data["frame_map"][frame])

        stack_id = None
        for frame_id in reversed(frame_ids):
            stack_key = (stack_id, frame_id)
            if stack_key not in thread_data["stack_map"]:
                stack_index = len(thread_data["stacks"])
                thread_data["stack_map"][stack_key] = stack_index

                gecko_stack = GeckoStack(
                    prefix_id=stack_id,
                    frame_id=frame_id
                )
                thread_data["stacks"].append(gecko_stack)
                stack_id = stack_index
            else:
                stack_id = thread_data["stack_map"][stack_key]

        gecko_sample = GeckoSample(
            stack_id=stack_id,
            time_ms=timestamp * 1000,
            event_delay=0.0
        )
        thread_data["samples"].append(gecko_sample)

    def _get_frame_category(self, frame):
        """
        Determine frame category based on frame information.
        Frame is a tuple: (filename, lineno, funcname)
        Can be extended to use any part of the frame for categorization.

        TODO: Change this once frames have a type/category field
        """
        filename, _, _ = frame
        if filename and filename.endswith('.py'):
            return PYTHON_CATEGORY
        else:
            return OTHER_CATEGORY

    def build_profile(self):
        threads = []

        for thread_id, thread_data in self.threads_data.items():
            frame_data = [frame.to_array() for frame in thread_data["frames"]]
            stack_data = [stack.to_array() for stack in thread_data["stacks"]]
            sample_data = [sample.to_array() for sample in thread_data["samples"]]

            gecko_thread = GeckoThread(
                name=f"Thread {thread_id}",
                isMainThread=(thread_id == 0),
                processType="default",
                processName="Python",
                processStartupTime=(self.start_time or 0) * 1000,
                processShutdownTime=None,
                registerTime=0,
                unregisterTime=None,
                pausedRanges=[],
                pid=thread_id,
                tid=thread_id,
                stringTable=self.string_table.get_strings(),
                frameTable=GeckoFrameTable(data=frame_data),
                stackTable=GeckoStackTable(data=stack_data),
                samples=GeckoSamples(data=sample_data),
                markers=GeckoMarkers(data=[])
            )

            threads.append(gecko_thread.to_dict())

        gecko_meta = GeckoMeta(
            startTime=(self.start_time or 0) * 1000
        )

        gecko_profile = GeckoProfile(
            meta=gecko_meta.to_dict(),
            libs=[],
            pages=[],
            pausedRanges=[],
            threads=threads,
            processes=[]
        )

        return gecko_profile.to_dict()
