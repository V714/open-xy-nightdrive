# Publishing Nightdrive 1.0.0

Nightdrive stays in this repository. The Open-XY contribution changes only `catalog/machines.json`
in a pull request targeting `etcetc000/open-xy:staging`. Do not copy the engine into the SDK.
The official [catalog guide](https://github.com/etcetc000/open-xy/blob/staging/docs/catalog.md)
is the authority for submission status and review.

## Publish the source

Create an empty GitHub repository named `open-xy-nightdrive` under **V714**, without generated
README, license or gitignore files. From this repository:

```sh
git remote add origin https://github.com/V714/open-xy-nightdrive.git
git push -u origin main
```

Wait for **Machine checks** to pass on GitHub, including the Windows native compile.
Then tag that exact commit and publish it:

```sh
git tag -a v1.0.0 -m "Nightdrive 1.0.0"
git push origin v1.0.0
git rev-parse 'v1.0.0^{commit}'
```

Keep the final command's full 40-character SHA. If review needs code changes after tagging,
use a new version rather than moving an existing public tag.

## Request review

Check that `nightdrive` is still unused in the catalog. It was unused when this repository was
prepared. The catalog can reserve an ID first with:

```json
{ "id": "nightdrive", "kind": "synth", "maintainer": "V714", "status": "reserved" }
```

For a tagged release submitted for review, propose the following entry, replacing `FULL_COMMIT_SHA`
with the tag's actual commit. `draft` pins the source for review without including it in released
firmware; promotion to `published` belongs to the review process.

```json
{
  "id": "nightdrive",
  "kind": "synth",
  "maintainer": "V714",
  "status": "draft",
  "repository": "https://github.com/V714/open-xy-nightdrive",
  "tag": "v1.0.0",
  "commit": "FULL_COMMIT_SHA",
  "path": "."
}
```

Open the PR against **staging**, changing only `catalog/machines.json`. Link the passing workflow
and its preview artifact. Describe Speed (ratio), Tension (feedback), Drift (modulation and transient),
and Glare (overdrive and Punch), and the note-driven animation. The code and artwork are MIT licensed
by Tomasz Lipczyński (V7 / GitHub V714), with the preserved Open-XY notice.
