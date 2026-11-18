# Publishing readable release notes

Every firmware release has full notes in `docs/RELEASE_VERSION.md` and a `RELEASE_NOTES.md` attachment. Users should be able to read changes on GitHub without downloading anything.

- Historical GitHub release pages contain the full Markdown notes directly in their descriptions.
- The current latest release has concise, readable inline change bullets and an online link to its detailed Markdown notes. This is necessary because older installed firmware limits GitHub's complete release-metadata response to 8 KiB. Long latest-release descriptions would prevent those boards from discovering upgrades.
- [CHANGELOG.md](../CHANGELOG.md) indexes every published firmware release. Signed packages and checksums remain separate assets.

## New releases

1. Write `docs/RELEASE_VERSION.md`: changes, relevant behavior and validation/limitations. Do not reuse stale hardware claims from an old release.
2. Write a short Markdown summary file containing actual readable changes, not only a link. Include material limitations. About 150-200 characters is a useful starting point; the actual complete response budget is checked, not assumed.
3. Build, test and sign through `tools/release_firmware.py`. For its GitHub CLI draft path, use `--create-draft --summary-file PATH`. The helper puts the summary directly in the page description and attaches all four assets, including the full notes. It checks the draft metadata size after upload. It does not publish the draft automatically.
4. Publish through the verified release workflow after the checks pass. Keep the anonymous `/releases/latest` JSON response below 8192 bytes (the helper's target is at most 8160). Do not add extra release assets casually: asset metadata counts toward that same response size.
5. Expand the now-historical pages and update the changelog with the script below. Commit the updated changelog and link it from the project page.

## Synchronize existing pages

Requires an authenticated [GitHub CLI](https://cli.github.com/). Run from the repository root:

```powershell
python tools/test_release_notes.py
python tools/update_release_pages.py --latest-summary captures/latest-summary.md
python tools/update_release_pages.py --latest-summary captures/latest-summary.md --apply --backup captures/release-pages-before.json
```

The first synchronization command is a dry run. The apply command saves the previous public release metadata to a new local backup file, writes notes into published page descriptions, and updates `CHANGELOG.md`. Draft releases are left alone. It verifies asset identity and keeps the same latest release. It does not change tags, packages, checksums, release publication status or any board settings.

Future publishers using another API wrapper should call the same formatting/size functions in `tools/release_notes.py`; do not clone the old one-line release descriptions. Full notes live in the repository for online reading even when the latest page needs a compact description.
