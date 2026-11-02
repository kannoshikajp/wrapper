# wrapper

A lightweight Windows C++ process launcher that runs a target executable inside a wrapped environment (machine environment + custom environment variables) configured via an INI file (`config.ini`).

## Features

- **Environment Wrapping**: Preserves all existing machine and user environment variables while setting or overriding custom variables from `config.ini`.
- **`%DIR%` Macro**: Automatically provides `%DIR%`, pointing to the directory where `wrapper.exe` resides.
- **Variable Expansion**: Expands existing environment variables (e.g. `%PATH%`, `%USERPROFILE%`, `%DIR%`) in executable paths, arguments, working directories, and environment values.
- **Transparent Execution**: Forwards standard I/O (stdin/stdout/stderr), passes console signals (`Ctrl+C`), and returns the exact exit code of the target process.
- **Flexible Overrides**: Forward CLI arguments to the target process or override `config.ini` path via `-c / --config`.
- **Zero External Dependencies**: Pure C++17 and native Win32 APIs (`CreateProcessW`, `SetEnvironmentVariableW`, `ExpandEnvironmentStringsW`).

## Configuration (`config.ini`)

Place `config.ini` in the same directory as `wrapper.exe` (or specify via `-c`):

```ini
[target]
; Executable to launch (supports full path, PATH lookup, and %DIR%)
executable = cmd.exe

; Default arguments passed to the target
arguments = /c echo Running from %DIR% with FOO=%FOO%

; Optional working directory (leave empty to inherit current working directory)
working_dir = %DIR%

; Print launch diagnostics (true / false)
verbose = false

[env]
; Custom environment variables
FOO = HelloFromWrapper
BAR = CustomValue

; Reference the wrapper directory
WRAPPER_HOME = %DIR%

; Prepend or append to existing machine PATH
PATH = %DIR%\bin;%PATH%
```

## Usage

```powershell
# Run using config.ini in wrapper's directory
wrapper.exe

# Pass additional arguments to target executable
wrapper.exe --extra-arg value

# Use a custom configuration file
wrapper.exe -c "C:\path\to\custom.ini"

# Run with verbose diagnostic output
wrapper.exe -v
```

### CLI Options

| Option | Description |
|---|---|
| `-c, --config <file>` | Path to a custom INI configuration file |
| `-v, --verbose` | Print environment and process launch diagnostics |
| `-h, --help` | Display help message |

## Building

Requires Visual Studio 2022 (v143 toolset) with C++ Desktop Development.

### Using MSBuild

```powershell
# Release build (x64)
msbuild wrapper.sln /p:Configuration=Release /p:Platform=x64

# Output binary and config.ini are copied to:
# bin\x64\Release\wrapper.exe
# bin\x64\Release\config.ini
```

### Using Visual Studio GUI

1. Open `wrapper.sln` in Visual Studio 2022.
2. Select `Release` and `x64` (or `x86`).
3. Build Solution (`Ctrl+Shift+B`).
4. `config.ini` is automatically copied to the build output folder on post-build.

## License

[MIT](LICENSE)
