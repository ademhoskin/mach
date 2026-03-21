test-cpp-bin := if os() == "windows" { "build\\mach_tests.exe" } else { "./build/mach_tests" }

build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel

test-cpp: build
    {{test-cpp-bin}}

install: build
    uv pip install -e .

test-python: install
    uv run pytest tests/python/ -v

test: test-cpp test-python
