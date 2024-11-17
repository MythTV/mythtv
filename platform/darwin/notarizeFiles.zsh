#!/bin/zsh
#
# Copyright (C) 2022-2024 John Hoyt
#
# See the file LICENSE_FSF for licensing information.
#

NOTA_FILE=$1
KEYCHAIN_PROFILE=$2

if [[ -z $NOTA_FILE ]]; then
  echo "No File to notarize specified."
  exit 2
fi

if [[ -z $KEYCHAIN_PROFILE ]]; then
  echo "No keychain profile specified.\n"
  echo "This specifies the Keychain where valid apple notarization credentials are stored."
  echo "The keychain includes the apple-id, team-id, and the Apple Generated APP_PWD\n"
  echo "These can be stored by running the following command:"
  echo "xcrun notarytool store-credentials KEYCHAIN_NAME"
  echo "      --apple-id YOUR_APPLE_ID"
  echo "      --team-id=YOUR_TEAM_ID"
  echo "      --password YOUR_APP_PWD\n"
  echo "The App Identifier can be obtained at https://developer.apple.com/"
  echo "    (Account -> Certificates, Identifiers & Profiles -> Identifiers -> + -> App IDs)"
  echo "The Team ID can be obtained at https://developer.apple.com/account"
  echo "    Account -> Scroll down to Membership Details -> Look for Team ID"
  echo "The App Password can be obtained at https://appleid.apple.com/account/manage"
  echo "    (Sign In -> APP-SPECIFIC PASSWORDS -> + )"
  exit 2
fi

# Check is this is an app or distribution
IS_APP=false
# If its an app, the file needs to be zipped via ditto
# and that file notatized
if [[  ${NOTA_FILE##*.} == "app" ]]; then
  IS_APP=true
  echo "Creating file for submission"
  /usr/bin/ditto -c -k --keepParent "$NOTA_FILE" "$NOTA_FILE.zip"
  ORIG_APP_FILE=${NOTA_FILE}
  NOTA_FILE="${NOTA_FILE}.zip"
fi

# sumbit file for notarization
echo "Submitting file for notarization"
notaOutput=$(xcrun notarytool submit ${NOTA_FILE} --keychain-profile ${KEYCHAIN_PROFILE} --wait)

# if this is an App, delete the temp zip file and update the NOTA_FILE
# variable to point back to the original App
if $IS_APP; then
  rm ${NOTA_FILE}
  NOTA_FILE=${ORIG_APP_FILE}
fi

#parse the status output
STATUS=$(echo "$notaOutput" | grep "status:"| gsed 1d | gsed 's/^.*status: *//')
echo "$STATUS"
case $STATUS in
  *Accepted*)
    echo '\033[0;32m'" +++++ Notaization Accepted"'\033[m'
      xcrun stapler staple "${NOTA_FILE}"
      echo '\033[0;32m'"+++++ ${NOTA_FILE} Notarization Success"'\033[m'
      returnCode=0
    ;;
  *)
    echo '\033[0;31m'"----- ${NOTA_FILE} Notarization Failud or Timeout"'\033[m'
    echo $notaOutput
    returnCode=1
esac

if [[ $NOTA_FILE =~ .*".app" ]]; then
  # if properly notarized, verify that app will pass gateway inspection of system policies
  echo "Verifying system policy compliance"
  spctl --assess --verbose ${NOTA_FILE}
fi
exit $returnCode