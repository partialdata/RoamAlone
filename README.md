# RoamAlone

A single-player van-life horror prototype built with Unreal Engine 5.8 and C++/Blueprints.

## Local setup

Install Unreal Engine 5.8 (currently developed with 5.8.4), its C++ build toolchain,
and Git LFS before cloning. Rider is the project's C++ editor.

```powershell
git lfs install
git clone https://github.com/partialdata/RoamAlone.git
cd RoamAlone
git lfs pull
```

Open `RoamAlone.uproject` in Unreal or Rider and build the editor target as needed.
Install RiderLink through Rider when prompted; its local plugin copy is ignored.
Read `AGENTS.md` before making gameplay changes.

## Version control

Commit `.gitignore` and `.gitattributes` together with project source, config,
required content (including external actor/object packages), and `.uproject`.
Unreal maps/assets and common binary art/audio/video formats use Git LFS.
Generated builds, caches, local IDE files, and `Saved` backups are ignored.
Keep required project-owned plugins and platform resources in version control.

Before committing or pushing assets, review `git status`, `git lfs status`, and
`git lfs ls-files`. A normal `git push` uses the LFS pre-push hook to upload the
corresponding LFS objects. Each new clone needs Git LFS installed locally.
