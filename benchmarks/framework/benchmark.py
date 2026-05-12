"""
Benchmark framework base class.

Each benchmark subclass defines:
  - Parameter space (names, default ranges)
  - Strategies per engine (mengine, coq, lean)
  - Code generators for each strategy
  - How to invoke each engine
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Optional
import os
import tempfile


@dataclass
class Strategy:
    """A specific strategy for a specific engine."""
    engine: str           # "mengine", "coq", "lean"
    name: str             # e.g. "repeat_rewrite", "simp_only"
    label: str            # display label for plotting, e.g. "Rocq: repeat rewrite"
    color: str = "black"
    marker: str = "o"
    variant: str | None = None


@dataclass
class ParamSpec:
    """Specification for a single parameter dimension."""
    name: str
    start: int
    stop: int
    step: int

    def range(self):
        return range(self.start, self.stop, self.step)

    def override(self, start=None, stop=None, step=None):
        """Return a new ParamSpec with overridden values."""
        return ParamSpec(
            name=self.name,
            start=start if start is not None else self.start,
            stop=stop if stop is not None else self.stop,
            step=step if step is not None else self.step,
        )


class Benchmark(ABC):
    """Base class for all benchmarks."""

    @property
    @abstractmethod
    def name(self) -> str:
        """Short identifier, e.g. 'rewrite_fa'."""
        ...

    @property
    @abstractmethod
    def description(self) -> str:
        """Human-readable description."""
        ...

    @property
    @abstractmethod
    def params(self) -> list[ParamSpec]:
        """Default parameter specifications (global range, same for all engines)."""
        ...

    @property
    @abstractmethod
    def strategies(self) -> list[Strategy]:
        """All strategies across all engines."""
        ...

    @property
    def x_label(self) -> str:
        """X-axis label for plotting. Override in subclass."""
        if self.params:
            return f"{self.params[0].name}"
        return "n"

    @property
    def y_label(self) -> str:
        return "Time (seconds)"

    @abstractmethod
    def generate(self, strategy: Strategy, params: dict[str, int], workdir: str) -> Optional[str]:
        """
        Generate the input file for a given strategy and parameters.
        
        For file-based engines (coq, lean): write a file to workdir and return its path.
        For mengine: return None (mengine uses CLI args).
        """
        ...

    @abstractmethod
    def get_command(self, strategy: Strategy, params: dict[str, int],
                    engine_path: str, generated_file: Optional[str],
                    config=None) -> list[str]:
        """
        Return the command line to execute for timing.
        """
        ...

    def result_key(self, strategy: Strategy, params: dict[str, int]) -> str:
        """Generate a unique key for storing results."""
        param_parts = "_".join(f"{k}{v}" for k, v in sorted(params.items()))
        return f"{strategy.engine}_{strategy.name}_{param_parts}"

    def get_strategies_for_engine(self, engine: str) -> list[Strategy]:
        """Return all strategies for a given engine."""
        return [s for s in self.strategies if s.engine == engine]
