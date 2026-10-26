# Publishing Nightdrive 1.0.1

Nightdrive stays in this repository. The Open-XY contribution changes only
`catalog/machines.json` in a pull request targeting `etcetc000/open-xy:staging`.
The official [catalog guide](https://github.com/etcetc000/open-xy/blob/staging/docs/catalog.md)
is the authority for submission and hardware profiling requirements.

## Publish the source

This update adds device-saved presets and requires SDK 1.3. Push the source:

```sh
git push origin main
```

Wait for **Machine checks** to pass, including the Windows native compile.
Then tag that exact tested commit:

```sh
git fetch origin --tags
git tag -a v1.0.1 -m "Nightdrive 1.0.1: bundled presets"
git push origin v1.0.1
git rev-parse 'v1.0.1^{commit}'
```

Use a new version if a tag already exists; never move a public tag. Keep the
full commit SHA from the last command.

## Update the catalog submission

Update the existing [Nightdrive PR #112](https://github.com/etcetc000/open-xy/pull/112)
on its source branch `V714:patch-1`. In the `nightdrive` entry of
`catalog/machines.json`, set `tag` to `v1.0.1` and `commit` to the full tested
commit SHA. Preserve its ID, repository, root path `.` and review status.
Commit that manifest edit on the same branch to update the existing PR.
Only the catalog entry belongs in Open-XY; the presets remain in this repository.

Mention that this update adds four presets using the SDK 1.3 `presets` field
without changing DSP code. Follow the catalog's current requirements for any
revision-bound hardware CPU readout; host estimates do not replace it.
The public builder picks up the new files after catalog acceptance and deployment.
For local testing, export a fresh personal kit with SDK 1.3 and this machine.
