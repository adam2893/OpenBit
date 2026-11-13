# Native Recovery Atlas

The Pages artifact is a static dashboard with a compressed data snapshot in `snapshot-parts/`. It contains no APK or native library. `build_data.py` refreshes the snapshot from the local `_re` workspace before the updated files are published.

The workflow template in `pages-workflow.yml` belongs at the repository root as `.github/workflows/pages.yml`. It publishes `_re/site/` from the `main` branch. The workflow removes the uncompressed local `data.json`; the browser reads the gzip snapshot chunks instead.

The dashboard reports repository records, not overall decompilation coverage. The complete method denominator, target execution, runtime equivalence, and decomp-to-C++ source acceptance remain explicit open gates.
