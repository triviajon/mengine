"""Benchmark registry with automatic benchmark discovery."""

from importlib import import_module
import inspect
import pkgutil
import os

from framework.benchmark import Benchmark


def _iter_benchmark_modules():
    package_name = __package__ or "benchmarks"
    for module_info in pkgutil.iter_modules((os.path.dirname(__file__),)):
        module_name = module_info.name
        if module_name.startswith("_") or module_name == "registry":
            continue
        yield import_module(f"{package_name}.{module_name}")


def _iter_benchmark_classes():
    for module in _iter_benchmark_modules():
        for _, cls in inspect.getmembers(module, inspect.isclass):
            if cls is Benchmark:
                continue
            if cls.__module__ != module.__name__:
                continue
            if not issubclass(cls, Benchmark) or inspect.isabstract(cls):
                continue
            yield cls


def _load_benchmarks():
    benchmarks = {}
    for benchmark_cls in sorted(_iter_benchmark_classes(), key=lambda cls: (cls.__module__, cls.__name__)):
        benchmark = benchmark_cls()
        if benchmark.name in benchmarks:
            raise ValueError(f"Duplicate benchmark name detected: {benchmark.name}")
        benchmarks[benchmark.name] = benchmark
    return benchmarks


ALL_BENCHMARKS = _load_benchmarks()
