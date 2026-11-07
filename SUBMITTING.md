# Publishing Nightdrive 1.0.3

The local `main` branch and annotated `v1.0.3` tag identify the release. Previous
tags are unchanged. Source, tests, artwork and presets belong to this repository.

```sh
git push origin main
git push origin v1.0.3
```

Wait for Machine checks, including the Windows native compile. A GitHub release
can use the 1.0.3 changelog entry. Never attach stock firmware, stock artwork or
a firmware-containing personal builder.

The prepared Open-XY fork branch is `catalog/nightdrive-v1.0.3`, based on official
`staging`. It changes only Nightdrive's entry in `catalog/machines.json`, pins
the release tag and full commit, and includes the complete SDK-generated CPU
record. The record uses three existing uninstrumented captures after verifying
that every build-provenance machine file matches the measured package. The
release changes only Markdown relative to that measured source; SDK provenance
excludes Markdown. No capture, package or source manifest is edited to rebind it.

After pushing the engine release, push the catalog branch to the **V714/open-xy**
fork and open a PR against **etcetc000/open-xy:staging**, as requested for this
update. Follow the current SDK catalog guide and disclose both 3.917% heaviest
mean and 6.192% worst observed block: both exceed the 3.5% recommendation.

Hardware evidence and the same-SDK 1.0.1 comparison are summarized in
[PERFORMANCE.md](PERFORMANCE.md). The maintainer should complete a final listening,
automation, save/reload, preset and polyphony pass before publishing. The existing
hardware CPU sweep does not certify all projects, continuous automation or every
pitch/control combination.
