# Original DOS driver parameter trace

Q2 publishes a far entry point into Creative's `CSP.SYS`. Tracing only Q2 and START left a possible explanation for TFX's out-of-table positions: the driver might clamp or remap them. Executing the original driver rules out that explanation for both inspected driver versions.

## Input and extraction

The same pinned `CSPU.ZIP` used by the firmware fetcher contains `MAIN.PVL`. Its PVM directory describes `CSP.SYS` at compressed offset `0x5ac9`, length `0x19d9`; the next member starts at `0x74a2`. The bounded DCL stream expands to 10,873 bytes with SHA-256:

```
43e1267997574ab3a36bc6453a98453cd10676e3caed64ab6e2a9c7ccfc2fa91
```

This is a relocation-free MZ driver with a 32-byte header. Addresses below refer to the loaded image after that header. Its DOS device name is `CSPXXXX0`, matching Q2's lookup. The ordinary IOCTL path returns the far API entry at image offset `01ce`.

The earlier `CSP_UPG.ZIP` contains a different flat DOS `CSP.SYS`: 6,599 bytes, SHA-256 `af1e6d52cecc49cbe5e884f6a706c6dd7c74b3fc0ab34558993dc6af65e17c5d`. Its `INSTALL.001` PVM directory places the compressed file at `0x8e21`, length `0x1127`. The corresponding far entry is `0218`, dispatcher `0604` and parameter path `0930`. Its byte transmission agrees with the newer driver despite changes to the binary format and wrapper.

## Parameter path

In the newer MZ driver, the far entry copies the four 32-bit arguments to its private stack and calls the dispatcher at `0c2a`. Function `00080000` reaches `0faa`:

1. Validate allocation and the handle through `0bf6`.
2. Reject parameter argument exactly `00000080`.
3. Reject an operand larger than 255.
4. Narrow the parameter to its low byte.
5. Call `0a0a`, which writes SB16 command `05`, the operand byte and the parameter byte.
6. Write SB16 command `03`, read the response and compare it with the parameter byte.

There is no position-table lookup or position-specific range check in this path. The operand is sent unchanged. The initial Q2 values and later START position updates therefore reach the DSP without a driver-side clamp, wrap or special mapping of 33. Under the low-byte command API, `(parameter=02, operand=33)` produces bytes `05 21 02 03` at the SB16 command port.

The exact argument `00000080` is rejected, but larger parameter arguments are otherwise narrowed. For example, `00010080` reaches byte command `80`. This is an observed API detail, not behaviour an application should deliberately depend on.

## Original-code checks

```sh
python3 tools/check_creative_driver.py --archive research/cspu.zip
# Or supply the already extracted original driver:
python3 tools/check_creative_driver.py --driver /path/to/CSP.SYS
```

Unicorn is required only for this optional research check. The script accepts either upgrade archive or either extracted driver, identifying them by verified hashes. No original driver bytes are redistributed. It executes each original allocation call, then checks all 256 operands for parameters `00`, `01`, `02`, `03` and `e0`: **1,280 calls per driver, 2,560 in total**. It also checks invalid operands, invalid handles, the rejected command, an incorrect response and command narrowing in both drivers. The newer driver additionally has a reentrancy guard whose rejection is checked; the older far wrapper lacks that guard.

The test supplies the post-initialisation SB16 ports, an arbitrary handle seed, zero software delay-loop calibration, ready status and response echo. It does not execute DOS driver installation, simulate a real card's timing or execute the DSP. In both versions the far entry, private-stack switch, dispatcher, validation, byte-write loops and response checking are original unmodified x86 instructions. The checks establish what the driver sends under those explicit inputs.

## Consequence for TFX

Position 33 is authored in many TFX sample definitions, and the independently checked START controller can issue still larger sweep positions. Those are genuine values at the DSP boundary. The remaining question is how the newer DSP shift/address/control semantics interpret them. The analytical audio model continues to reject unsupported positions; it does not turn this driver finding into an unverified claim about the resulting audio.
