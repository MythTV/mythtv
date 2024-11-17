#!/bin/zsh
#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#

APP_BUNDLE=$1
ENTITLEMENT=$2
CS_ID=$3

if [[ -z $APP_BUNDLE ]]; then
  echo "No Application specified."
  exit 2
else
  # need a non-relpath to the APP Bundle
  APP_BUNDLE=$(readlink -f $APP_BUNDLE)
fi

if [[ -z $ENTITLEMENT ]]; then
  echo "No entitlement file specified."
  exit 2
fi

# check is the signing ID was passed in, if so sign with those credentials vs adhoc
adhoc_signing=false
if [ -z $CS_ID ]; then
   echo "No code signing ID specified."
   echo "Using AdHoc Signature"
   adhoc_signing=true
   CS_ID="-"
fi

# Per the Apple developer notes, you must codesign from the inside out roughly meaning
# start dylibs then frameworks then executables.  In reality, as long as the codesign
# timestamps are relavtively close, no need to trace the recursive tree from excutables
# down to dylibs
echo "Codesigning Shared Libraries"
find $APP_BUNDLE -type f \( -name "*.dylib" -o -name "*.so" \) \
  -exec /usr/bin/codesign --force --sign $CS_ID -v --deep --timestamp -o runtime --entitlements $ENTITLEMENT --continue {} \;

echo "Codesigning Framework Binaries"
find $APP_BUNDLE/Contents/Frameworks -type f -perm -111 ! -name "*.dylib" ! -name "*.so" \
  -exec /usr/bin/codesign --force --sign $CS_ID -v --deep --timestamp -o runtime --entitlements $ENTITLEMENT --continue {} \;

echo "Codesigning Executables"
find $APP_BUNDLE/Contents/MacOS -type f -perm -111 \
  -exec /usr/bin/codesign --force --sign $CS_ID -v --deep --timestamp -o runtime --entitlements $ENTITLEMENT --continue {} \;

# Sign the Frameworks Directories
echo "Codesigning Frameworks"
/usr/bin/codesign --force --sign $CS_ID -v --deep --timestamp -o runtime --entitlements $ENTITLEMENT --continue $APP_BUNDLE/Contents/Frameworks/*.framework

# Finally sign the application
echo "Codesigning Application"
/usr/bin/codesign --force --sign $CS_ID -v --deep --timestamp -o runtime --entitlements $ENTITLEMENT --continue $APP_BUNDLE

# verify that the codesigning took
echo "Verifying code signatures"
/usr/bin/codesign --verify -vv --deep $APP_BUNDLE
CS_STATUS=$?

if [[ $CS_STATUS == 0 ]]; then
  echo '\033[0;32m'"+++++ ${APP_BUNDLE} Signing Success"'\033[m'
  exit 0
else
  echo '\033[0;31m'"----- ${APP_BUNDLE} Signing Failue"'\033[m'
  exit 1
fi