# Publishing Nightdrive 1.0.2

Nightdrive source, tests, artwork and presets stay in this repository. Never move
an existing public tag. See [CHANGELOG.md](CHANGELOG.md) for release changes and
validation status.

## Publish the source

The local `main` branch and annotated `v1.0.2` tag identify the release. Push:

```sh
git push origin main
git push origin v1.0.2
```

Wait for **Machine checks**, including the Windows native compile. Publish a
GitHub release from `v1.0.2` using its changelog entry. Do not attach stock firmware,
stock artwork, firmware-containing packages or the private personal builder.

## Update the Open-XY catalog

Follow the current SDK [catalog guide](https://github.com/etcetc000/open-xy/blob/main/docs/catalog.md).
The source release and catalog acceptance are separate. Listening and automation
have been confirmed on OP-XY; a revision-bound hardware CPU report is still needed
before this CPU-affecting update can be submitted to the catalog.

Profile the tagged revision following the SDK's hardware profiling guide. Generate
`catalog-cpu.json` with `sdk.py profile report`; both average `block_percent` and
worst-block `peak_percent` must be present. Host estimates cannot replace them.

Once available, update only Nightdrive's entry in `catalog/machines.json`: set
`tag` to `v1.0.2`, `commit` to `git rev-parse 'v1.0.2^{commit}'`, and `cpu` to the
complete generated record. Preserve identity, repository, path and review status.
Use the target branch required by the current catalog guide; an SDK requirement
available only on staging targets staging. Review the full hardware checklist
for the exact tagged revision before submission.
