# Build TOPO from source

Run these commands from the TOPO repository root. You need Git, Python 3.11 or later and a C++11 host compiler. A native firmware build also needs a supported Blackfin toolchain and your own compatible OP-XY OS 1.2.1 image. TE Builder handles these inputs through its usual workflow. For a standalone build, use a fresh SDK checkout.

## Prepare the SDK

```sh
topo_dir="$PWD"
sdk_dir="$topo_dir/../open-xy-topo-sdk"
git clone https://github.com/etcetc000/open-xy.git "$sdk_dir"
git -C "$sdk_dir" checkout --detach ca122f4062f08f13a3023904620a4c35ff9b4a45
python3 tools/prepare_sdk.py "$sdk_dir"
```

The preparer verifies the SDK commit and the SHA-256 digest of each patch in `machine-bundle.json`. The patches provide the M1 surface renderer, single-cycle browser audition and authenticated arithmetic runtime support.

## Test on the host

```sh
python3 "$sdk_dir/sdk.py" test --machine "$topo_dir/userwt" --no-cpu
python3 tests/offline.py "$sdk_dir"
python3 tests/selector.py "$sdk_dir"
python3 tools/package_manager.py --check
```

The PCM test compares the oscillator with the source WAVs at both scan endpoints and exercises all four widths. It also checks invalid lengths and compares the display with the oscillator over 288 input combinations. The selector test covers first-cycle audition and the stock sampler path. These are host checks. CPU measurements require the device.

## Build a native candidate

Use the SDK’s supported host and toolchain. Substitute the path to your local stock image. The SDK writes to a fresh output directory.

```sh
stock_os="/absolute/path/to/os.ldr"
cd "$sdk_dir"
python3 sdk.py toolchain fetch
python3 sdk.py build --machine "$topo_dir/userwt" --stock-os "$stock_os" \
  --profiling off --out "$topo_dir/../topo-native-build"
```

On Apple silicon, the first build that needs the arithmetic runtime can fetch the SDK-pinned archive and verify its checksum. See the [pinned toolchain documentation](https://github.com/etcetc000/open-xy/blob/ca122f4062f08f13a3023904620a4c35ff9b4a45/docs/toolchain.md).

## Package a release

From the TOPO repository root, run `python3 tools/package_manager.py` and then `python3 tools/package_manager.py --check`. The script creates the standalone TE Builder source ZIP under `release/`. Push the repository folder contents, tag the release and attach that ZIP to the GitHub release. Keep the root manifest and hidden GitHub workflow when copying the folder. Use the repository clone URL for TE Builder's Git import.

For an on-device check, load Glass at K1 2048, then browse Vowel-256 at K1 256. Listen for the short C4 preview, confirm and sweep K2 while sustaining a note. Verify that the highlighted ridge moves across the bank, Warp changes the shape and a stock sampler still auditions its file normally.
