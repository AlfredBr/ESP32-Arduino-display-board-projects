# Compile and upload HelloScreen from PowerShell

## Exact compile command used during development

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn 'esp32:esp32:esp32c3:CDCOnBoot=cdc' --config-file 'C:\Users\alfre\.arduinoIDE\arduino-cli.yaml' 'C:\Users\alfre\Downloads\HelloScreen'
```

- `&` is PowerShell's call operator: it runs the quoted executable path.
- `compile` builds the sketch; by itself it does not upload anything.
- `--fqbn` chooses the board and compile options. `esp32:esp32:esp32c3` is
  ESP32C3 Dev Module; `CDCOnBoot=cdc` enables USB CDC On Boot so `Serial`
  output appears over USB.
- `--config-file` reuses Arduino IDE's installed board packages and sketchbook
  library paths. This locates your existing LovyanGFX library.
- The final argument is the folder containing HelloScreen.ino and BoardDisplay.h.
- Build output goes into Arduino CLI's default build/cache directories.
  This command does not export binaries into the sketch folder.

## Compile and upload in one command

Close Arduino IDE's Serial Monitor before an external upload so it can release
COM3. The IDE itself can remain open. Save any sketch edits before running CLI:
it reads files on disk, not unsaved text in the editor.

```powershell
& 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile --fqbn 'esp32:esp32:esp32c3:CDCOnBoot=cdc' --config-file 'C:\Users\alfre\.arduinoIDE\arduino-cli.yaml' --upload --port COM3 'C:\Users\alfre\Downloads\HelloScreen'
```

`--upload` flashes the board only after a successful compile. It replaces the
current sketch, and the board restarts afterward. No separate Run command is
needed. Avoid using `arduino-cli upload` alone for edited source: that command
does not compile the changes first.

## Script usage

The script defaults to the sketch folder it lives in. Keep it alongside the
.ino and header, even if you move the whole folder into the Arduino sketchbook.
It works with Windows PowerShell 5.1 or PowerShell 7.

```powershell
Set-Location 'C:\Users\alfre\Downloads\HelloScreen'

# Compile only (no board connection required).
.\Build-HelloScreen.ps1

# Compile, then upload to the connected board.
.\Build-HelloScreen.ps1 -Upload -Port COM3

# Show the exact upload command without compiling or flashing.
.\Build-HelloScreen.ps1 -Upload -Port COM3 -DryRun
```

If PowerShell reports that script execution is disabled, you can run this
locally created script in a separate PowerShell process without changing your
saved execution policy:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\alfre\Downloads\HelloScreen\Build-HelloScreen.ps1' -Upload -Port COM3
```

Optional parameters: `-SketchPath` (folder or .ino file), `-CliPath`,
`-ConfigFile`, `-Fqbn`, and `-Port`. Defaults match the working setup on this PC.
If you intentionally target a different board, change the FQBN and make sure
the sketch's display wiring also matches that hardware.

The script prints the command, streams compiler/uploader output, and exits
with code 0 on success or nonzero on failure. It never uploads unless `-Upload`
is supplied. `-DryRun` checks paths and prints arguments but performs no build
or upload.

## Useful diagnostic commands

```powershell
$cli = 'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
$config = 'C:\Users\alfre\.arduinoIDE\arduino-cli.yaml'

# Installed command-line tool version.
& $cli version

# Find the board's current COM port.
& $cli board list --config-file $config

# List installed ESP32-C3 board profiles.
& $cli board listall ESP32C3 --config-file $config

# Inspect the supported build and upload flags.
& $cli compile --help
& $cli upload --help
```

After uploading, reopen Serial Monitor on the current board port at 921600 baud.
If uploading reports that COM3 is busy, close any other serial-terminal program.
If the port changes after reconnecting USB, use the new port with `-Port`.

Development setup: Arduino IDE 2.3.10, bundled Arduino CLI 1.5.1,
Espressif ESP32 core 3.3.12, LovyanGFX 1.2.30. No separate CLI installation or
library update was needed.
