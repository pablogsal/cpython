from abc import ABC, abstractmethod


class Collector(ABC):
    @abstractmethod
    def collect(self, unwinder):
        """Collect profiling data from stack frames."""

    @abstractmethod
    def export(self, filename):
        """Export collected data to a file."""
