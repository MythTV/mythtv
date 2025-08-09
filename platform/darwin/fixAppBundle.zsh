#!/bin/zsh
#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#

# This application finds dylibs that macdeployqt misses correcting their dylib linkes
# to point internally to the APP Bundle.

APP_BUNDLE=$1
PKGMGR_PREFIX=$2
MYTHTV_INSTALL_PREFIX=$3

APP_CONTENTS_DIR=$APP_BUNDLE/Contents
APP_MACOS_DIR=$APP_BUNDLE/Contents/MacOS
APP_FMWK_DIR=$APP_BUNDLE/Contents/Frameworks
APP_PLUGINS_DIR=$APP_BUNDLE/Contents/PlugIns

checkID(){
  libFile=$1
  # use otool to make sure the passed in file doesn't have PKGMGR_PREFIX or MYTHTV_INSTALL_PREFIX
  # in the ID
  libID=$(echo $(otool -D $libFile)| sed 's^.*: ^^')
  case $libID in
    $PKGMGR_PREFIX*|$MYTHTV_INSTALL_PREFIX*)
      newID=${libFile#$APP_CONTENTS_DIR/}
      newID="@executable_path/../$newID"
      echo '\033[0;36m'"    installLibs: Correcting ID for $libFile"'\033[m'
      NAME_TOOL_CMD="install_name_tool -id $newID $libFile"
      eval "${NAME_TOOL_CMD}"
    ;;
  esac
}

checkRPATH(){
  libFile=$1
  libRPATH=$(/usr/bin/otool -l "$libFile"|grep -e "@loader_path/../lib")
  if [ -n "$libRPATH" ]; then
    echo '\033[0;36m'"    installLibs: Adding LC_RPATH to $libFile"'\033[m'
    NAME_TOOL_CMD="install_name_tool -rpath \"@loader_path/../lib\" \"@loader_path/../Frameworks\" $libFile"
    eval "${NAME_TOOL_CMD}"
  fi

}

installLibs(){
  binFile=$1
  # check if there are any shared libraries the are still linked against PKGMGR_PREFIX or MYTHTV_INSTALL_PREFIX
  pathDepList=$(/usr/bin/otool -L "$binFile"|grep -e loader_path -e "$PKGMGR_PREFIX" -e "$MYTHTV_INSTALL_PREFIX")
  pathDepList=$(echo "$pathDepList"| sed 's/.*://' | sed 's/(.*//')
  if [ -z "$pathDepList" ] ; then return; fi
  loopCTR=0
  while read -r dep; do
    if [ "$loopCTR" = 0 ]; then
      echo '\033[0;36m'"    installLibs: Parsing $binFile for linked libraries"'\033[m'
    fi
    loopCTR=$loopCTR+1
    lib=${dep##*/}

    # Parse the lib if it isn't null
    if [ -n "$lib" ]; then
      #check if it is already installed in the framewrk, if so update the link
      needsCopy=false
      recurse=false
      inAPPlib=$(find "$APP_FMWK_DIR" "$APP_PLUGINS_DIR" -name "$lib" -print -quit)
      case $inAPPlib in
        *Frameworks*|*PlugIns*)
          newLink="@executable_path/../${inAPPlib#$APP_CONTENTS_DIR/}"
        ;;
        # if not in the APP, check special handling or copy in recursively if necessary
        *)
          newLink="@executable_path/../Frameworks/$lib"
          needsCopy=true
          case $lib in
            # homebrew incorrectly appends _arm64 when linking libhdhomerun
            libhdhomerun*)
              sourcePath=$(find "$PKGMGR_PREFIX" -name "libhdhomerun*.dylib" -print -quit)
            ;;
            # homebrew changes the cryptography path into a filname for some reason
            cryptography*_rust.abi3.so)
              sourcePath=$(find "$PKGMGR_PREFIX" -name "*_rust.abi3.so" -print -quit)
            ;;
            *)
              sourcePath=$(find "$MYTHTV_INSTALL_PREFIX" "$PKGMGR_PREFIX" \( -name "$lib" -o -name "${binFile%%.*}.${lib##*.}" \) -print -quit)
            ;;
          esac
          # Copy in any missing files
          if $needsCopy; then
            echo '\033[0;34m'"      +++ installLibs: Installing $lib into app"'\033[m'
            cp -RHn "$sourcePath" "$APP_FMWK_DIR/$lib"
            checkID "$APP_FMWK_DIR/$lib"
            checkRPATH "$APP_FMWK_DIR/$lib"
            # check the new library recursively
            recurse=true
          fi
        ;;
      esac
      # update the link in the app/executable to the new interal Framework
      echo '\033[0;34m'"      --- installLibs: Updating $binFile link for $lib"'\033[m'
      # it should now be in the App Bundle Frameworks, we just need to update the link
      NAME_TOOL_CMD="install_name_tool -change $dep $newLink $binFile"
      eval "${NAME_TOOL_CMD}"
      # If a new lib was copied in, recursively check it
      if  $needsCopy && $recurse ; then
        echo '\033[0;34m'"      ^^^ installLibs: Recursively install $lib"'\033[m'
        installLibs "$APP_FMWK_DIR/$lib"
      fi
    fi
  done <<< "$pathDepList"
}

fixMissingQt(){
  # occasionally macdeployqt misses some linked Qt frameworks that are rpathd in a non-standard way
  # We need to find the missing frameworks, copy them into the app bundle, and link them appropriately

  # find all @rpath Qt Frameworks
  QT_FMWKS=$(otool -L $APP_FMWK_DIR/Qt*.framework/Versions/Current/Qt*|grep "@rpath"|grep "Qt.*framework"|sort -u|sed 's^.*@rpath/^^'  | sed 's^\.framework.*^.framework^')
  while read qtFMWK; do
    if [ ! -d $APP_FMWK_DIR/$qtFMWK ]; then
      echo '\033[0;34m'"      +++ installLibs: Installing $qtFMWK into app"'\033[m'
      # copy in the Qt* framework and set variables
      sourcePath=$(find "$PKGMGR_PREFIX" -name "$qtFMWK" -print -quit)
      cp -RHn "$sourcePath" "$APP_FMWK_DIR/"
      libName=${qtFMWK%".framework"}
      # update the framework's library file ID to point to its new position in the app bundle
      # install name tool needs the real file, so use greadlink to get the final path from the symlink
      libFile=$(greadlink -f $APP_FMWK_DIR/$qtFMWK/$libName)
      newID="@executable_path/../Frameworks/$qtFMWK/Versions/Current/$libName"
      NAME_TOOL_CMD="install_name_tool -id $newID $libFile"
      eval "${NAME_TOOL_CMD}"
      # update links to (and copy in missing) any linked libraries required by the framework
      installLibs $libFile
    fi
  done <<< "$QT_FMWKS"
}

# Fix any Qt Frameworks missed by macdeployqt
fixMissingQt
# Look over all dylibs in the APP Bundle and correct any dylib path's that macdeployqt misses.
for fileName in $APP_CONTENTS_DIR/**/*(.); do
  if [[ -d $fileName || -L $fileName ]]; then continue; fi
  tempFileName=${fileName##*/}
  tempFileType=${tempFileName#.*}
  # We only want binaries which are .dylib, .so, or have no file extension
  if [[ $fileName =~ ".*\.(dylib|so)" || ! $tempFileType == *"."* ]]; then
    # last check to make sure the file is a binary
    if [[ $(file $fileName) =~ ".*"Mach-O".*" ]]; then
      case $fileName in
        *Frameworks*|*PlugIns*)
          checkID $fileName
          checkRPATH $fileName
        ;;
      esac
      installLibs "$fileName"
    fi
  fi
done
