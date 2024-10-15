# FFmpeg Update Instructions

FFmpeg was last updated to FFmpeg 7.1 `5c59d97e8ab90daefa198dcfb0ee4fdf9c57e37f` on 2024-10-12,
which branched from FFmpeg master at `e1094ac45d3bc7942043e72a23b6ab30faaddb8a`.


## Branches in MythTV/FFmpeg:
`master` is `ffmpeg/master`.

`release/x.y` is all of the MythTV changes rebased onto `ffmpeg/release/x.y`.

`release/mythtv/x.y` is the previous `rebase/a.b` plus all of ffmpeg's commits from `ffmpeg/release/x.y` cherry-picked on top.

`rebase/x.y` is `release/x.y` rebased onto the branch point from `master`.

The `release/mythtv` branches are for bisecting the FFmpeg changes when updating FFmpeg in MythTV.

The `release` branches will allow us to easily see what are changes are, making it easier to upstream them.

`git diff release/x.y release/mythtv/x.y` should show they are identical when initially created.

Commits to `mythtv/external/FFmpeg` after the update shall be added to `release/x.y`
and shall include a link to the mythtv commit in the
`https://github.com/MythTV/mythtv/commit/<full hash>` format.


## Notes:

This document assumes a remote named `ffmpeg` pointed at either `https://git.ffmpeg.org/ffmpeg.git`
or `https://github.com/ffmpeg/ffmpeg`.

`MYTHTV_GIT_DIR` and `FFMPEG_GIT_DIR` will be used as placeholders for the git directories
for MythTV and FFmpeg, respectively.

## Update Instructions

### Step 0: Ensure `mythtv/external/FFmpeg` is identical to the latest release branch

```
cd $MYTHTV_GIT_DIR
git checkout master
git pull
git clean -xdf
git checkout -b ffmpeg/x.y
mv mythtv/external/FFmpeg mythtv/external/FFmpeg1
cd $FFMPEG_GIT_DIR
git checkout release/a.b
git pull
git clean -xdf
cd $MYTHTV_GIT_DIR
cp -r $FFMPEG_GIT_DIR mythtv/external/FFmpeg
git status
rm -rf mythtv/external/FFmpeg
mv mythtv/external/FFmpeg1 mythtv/external/FFmpeg
```

If `git status` showed any modified files or any untracked files other than mythtv/external/FFmpeg1:
Find the last commit added to FFmpeg:
```
cd $FFMPEG_GIT_DIR
git log
cd $MYTHTV_GIT_DIR
git log -- mythtv/external/FFmpeg
git format-patch <commit> -- mythtv/external/FFmpeg
```

Remove diff sections from each patch that change files not in mythtv/external/FFmpeg

Append
```

From:
https://github.com/MythTV/mythtv/commit/<commit hash>
```
to each commit message.

```
cp *.patch $FFMPEG_GIT_DIR
cd $FFMPEG_GIT_DIR
git am -p4 *.patch
git push
```

### Step 1: Rebase `release/x.y` onto `ffmpeg/master`

```
cd $FFMPEG_GIT_DIR
git checkout master
git pull ffmpeg master
git push
# create a new branch rebase/a.b on master at the branch point for ffmpeg/release/a.b
git checkout -b rebase/a.b <commit hash of branch point of ffmpeg/release/a.b on master>
# cherry-pick our changes from release/a.b
git cherry-pick ffmpeg/release/a.b..release/a.b
git push
```

### Step 2: Add the new FFmpeg commits

```
# create a new branch release/mythtv/x.y
git checkout -b release/mythtv/x.y
# cherry-pick the commits from ffmpeg/release/x.y into release/mythtv/x.y
git cherry-pick ..ffmpeg/release/x.y
git push
```

If you want to ensure mpegts-mythtv compiles, instead of
```
git cherry-pick ..ffmpeg/release/x.y
```
, do
```
git log --reverse -p ..ffmpeg/release/x.y -- libavformat/mpegts.c libavformat/mpegts.h
```
and cherry-pick until each commit in turn and create a new commit copying the changes to
`mpegts-mythtv.c` and `mpegts-mythtv.h`.

For each commit:
```
git cherry-pick <commit hash of last commit from FFmpeg>..<commit hash>
git log -p -- libavformat/mpegts.c libavformat/mpegts.h
git format-patch HEAD~ -- libavformat/mpegts.c libavformat/mpegts.h
sed -i "s%libavformat/mpegts%libavformat/mpegts-mythtv%g" *.patch
git am *.patch
git log -p
rm *.patch
```

and finally:
```
git cherry-pick <commit hash of last commit from FFmpeg>..ffmpeg/release/x.y
```

### Step 3: Copying the updated FFmpeg to MythTV

After final compile test of FFmpeg:
```
cd $FFMPEG_GIT_DIR
git clean -xdf
cd $MYTHTV_GIT_DIR
git clean -xdf
git rm -r mythtv/external/FFmpeg/
rm -rf mythtv/external/FFmpeg/
cp -r $FFMPEG_GIT_DIR mythtv/external/FFmpeg
rm -rf mythtv/external/FFmpeg/.git
# ensure changed files look correct
git status
git add -A -- mythtv/external/FFmpeg
git commit
```

Create a separate commit updating this document with the new FFmpeg version, commit hashes,
and date.

Update MythTV to compile with the new version of FFmpeg.  It may be beneficial
for bisecting if the changes can be moved before the FFmpeg update commit, perhaps
using FFmpeg's various `version.h` headers.

#### Ensure FFmpeg can still use our included version of nv-codec-headers

The current version can be found in `nv-codec-headers/ffnvcodec.pc.in` and `nv-codec-headers/CMakeLists.txt`,
which should agree.

In `FFmpeg/configure` look for the section containing `if ! disabled ffnvcodec; then`
or `check_pkg_config ffnvcodec` and see if the current version is still allowed.

If the current version is no longer allowed by FFmpeg, update nv-codec-headers to an allowed version:
```
cd $MYTHTV_GIT_DIR/mythtv/external
mv nv-codec-headers/CMakeLists.txt nv-codec-headers_CMakeLists.txt
rm -rf nv-codec-headers
git clone https://git.videolan.org/git/ffmpeg/nv-codec-headers.git
cd nv-codec-headers
# optional: use a specific version depending on the release date
# of the minimum required driver versions in nv-codec-headers/README
git checkout <tag>

rm -rf .git
cd ..
# update the version in CMakeLists.txt to match the new version in nv-codec-headers/ffnvcodec.pc.in
# TODO: sed/awk command?
grep "Version: " nv-codec-headers/ffnvcodec.pc.in
nano nv-codec-headers_CMakeLists.txt
mv nv-codec-headers_CMakeLists.txt nv-codec-headers/CMakeLists.txt
git status
git add -A -- nv-codec-headers
git commit
```

This commit should be after the commit updating this document and before any commits modifying MythTV.

### Step 4: Testing

Post on the mythtv-dev mailing list that the new version is available for testing
after running whatever tests you can do yourself.


### Step 5: Rebasing FFmpeg modifications

This can be done simultaneously with testing:
```
cd $FFMPEG_GIT_DIR
git checkout release/mythtv/x.y
git checkout -b release/x.y
git rebase -i ffmpeg/release/x.y
# drop any commits that had merge conflicts during the cherry-pick
git diff release/mythtv/x.y
# redo the rebase until there is no difference in the final state

# merge/reorder mythtv commits as desired to create commits for upstreaming
git rebase -i ffmpeg/release/x.y
git diff release/mythtv/x.y
# release/x.y must still be identical to release/mythtv/x.y
```
