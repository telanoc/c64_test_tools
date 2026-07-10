# Test suite

Run all tests from the repository root:

```sh
./tests/run.sh
```

The only prerequisite is `uv`, which installs both `uv` and `uvx`. The runner
uses `uvx` for a pinned, isolated PlatformIO launcher and `uv run` for a managed
Python 3.12 VCD analyzer. Network access is needed on the first run to download
missing runtimes and pinned packages.

The AVR compiler, Arduino framework, simavr, build output, and VCD trace are
kept under the ignored `.pio-core/` and `.pio/` directories.

The suite performs three checks:

1. Compiles the production Mega 2560 sketch with its normal configuration.
2. Compiles the sketch with `DO_TESTING` to keep the optional timing diagnostics
   buildable.
3. Compiles a harness which includes that sketch and runs four complete sweeps
   of every constant write pattern, constant verification, random writes, and
   random verification in simavr. It reports each phase separately and checks
   from the `PORTA` and `/RAS` waveform that all 256 DRAM rows are refreshed and
   no same-row interval exceeds 4 ms. Verify sweeps use the simulated input
   pull-ups as an all-ones DRAM response so the success paths run to completion.

Remove all downloaded tools and generated files with:

```sh
rm -rf .pio .pio-core
```
