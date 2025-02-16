#
# Copyright (C) 2022-2023 David Hampton
#
# See the file LICENSE_FSF for licensing information.
#

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

add_executable(
  mythfrontend
  action.cpp
  action.h
  actionset.cpp
  audiogeneralsettings.cpp
  audiogeneralsettings.h
  backendconnectionmanager.cpp
  backendconnectionmanager.h
  channelrecpriority.cpp
  channelrecpriority.h
  customedit.cpp
  customedit.h
  custompriority.cpp
  custompriority.h
  editvideometadata.cpp
  editvideometadata.h
  exitprompt.cpp
  exitprompt.h
  galleryconfig.cpp
  galleryconfig.h
  galleryinfo.cpp
  galleryinfo.h
  galleryslide.cpp
  galleryslide.h
  galleryslideview.cpp
  galleryslideview.h
  gallerythumbview.cpp
  gallerythumbview.h
  gallerytransitions.cpp
  gallerytransitions.h
  galleryviews.cpp
  galleryviews.h
  globalsettings.cpp
  globalsettings.h
  grabbersettings.cpp
  grabbersettings.h
  guidegrid.cpp
  guidegrid.h
  idlescreen.cpp
  idlescreen.h
  keybindings.cpp
  keybindings.h
  keygrabber.cpp
  keygrabber.h
  manualschedule.cpp
  manualschedule.h
  mediarenderer.cpp
  mediarenderer.h
  mythcontrols.cpp
  mythcontrols.h
  mythfexml.cpp
  mythfexml.h
  mythfrontend.cpp
  mythfrontend_commandlineparser.cpp
  mythfrontend_commandlineparser.h
  networkcontrol.cpp
  networkcontrol.h
  playbackbox.cpp
  playbackbox.h
  playbackboxhelper.cpp
  playbackboxhelper.h
  playbackboxlistitem.cpp
  playbackboxlistitem.h
  playbackstate.cpp
  playbackstate.h
  prevreclist.cpp
  prevreclist.h
  progdetails.cpp
  progdetails.h
  progfind.cpp
  progfind.h
  proginfolist.cpp
  proginfolist.h
  proglist.cpp
  proglist.h
  proglist_helpers.cpp
  proglist_helpers.h
  programinfocache.cpp
  programinfocache.h
  programrecpriority.cpp
  programrecpriority.h
  schedulecommon.cpp
  schedulecommon.h
  scheduleeditor.cpp
  scheduleeditor.h
  servicecontracts/datacontracthelper.h
  servicecontracts/frontendActionList.h
  servicecontracts/frontendServices.h
  servicecontracts/frontendStatus.h
  servicecontracts/service.cpp
  servicecontracts/service.h
  serviceHosts/frontendServiceHost.h
  serviceHosts/servicehost.cpp
  serviceHosts/servicehost.h
  serviceHosts/wsdl.cpp
  serviceHosts/wsdl.h
  serviceHosts/xsd.cpp
  serviceHosts/xsd.h
  services/frontend.cpp
  services/frontend.h
  services/mythfrontendservice.cpp
  services/mythfrontendservice.h
  settingshelper.h
  setupwizard_audio.cpp
  setupwizard_audio.h
  setupwizard_general.cpp
  setupwizard_general.h
  setupwizard_video.cpp
  setupwizard_video.h
  statusbox.cpp
  statusbox.h
  themechooser.cpp
  themechooser.h
  upnpscanner.cpp
  upnpscanner.h
  videodlg.cpp
  videodlg.h
  videofileassoc.cpp
  videofileassoc.h
  videofilter.cpp
  videofilter.h
  videoglobalsettings.cpp
  videoglobalsettings.h
  videolist.cpp
  videolist.h
  videometadatasettings.cpp
  videometadatasettings.h
  videoplayercommand.cpp
  videoplayercommand.h
  videoplayersettings.cpp
  videoplayersettings.h
  videopopups.cpp
  videopopups.h
  viewscheduled.cpp
  viewscheduled.h
  viewschedulediff.cpp
  viewschedulediff.h)

target_include_directories(mythfrontend PRIVATE .)

target_compile_definitions(
  mythfrontend
  PRIVATE
    $<$<AND:$<BOOL:${WIN32}>,$<NOT:$<CONFIG:Debug>>>:WINDOWS_CLOSE_CONSOLE>)

list(
  APPEND
  EXTRA_PACKAGES
  $<TARGET_NAME_IF_EXISTS:Qt${QT_VERSION_MAJOR}::DBus>
  $<TARGET_NAME_IF_EXISTS:Qt${QT_VERSION_MAJOR}::Script>
  $<TARGET_NAME_IF_EXISTS:Qt${QT_VERSION_MAJOR}::ScriptTools>
  $<TARGET_NAME_IF_EXISTS:Qt5::WebEngine>
  $<TARGET_NAME_IF_EXISTS:Qt6::WebEngineQuick>
  # AndroidExtras only exists for Qt5
  $<TARGET_NAME_IF_EXISTS:Qt5::AndroidExtras>)

target_link_libraries(
  mythfrontend
  PUBLIC PkgConfig::LIBAVFORMAT
         Qt${QT_VERSION_MAJOR}::Core
         Qt${QT_VERSION_MAJOR}::Network
         Qt${QT_VERSION_MAJOR}::Sql
         Qt${QT_VERSION_MAJOR}::Widgets
         Qt${QT_VERSION_MAJOR}::Xml
         myth
         mythtv
         mythbase
         mythmetadata
         mythui
         mythupnp
         $<TARGET_NAME_IF_EXISTS:PkgConfig::LibCEC>
         ${EXTRA_PACKAGES})

install(TARGETS mythfrontend RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES MFEXML_scpd.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mythtv)

#
# The symbols in an executable files aren't normally published as part of the
# dynamic symbol table.  This causes a problem on android, where the java based
# program launcher need to lookup the the dynamic symbol "main" in order to
# start the program.  This option will force the compiler to publish the needed
# symbol in the dynamic table. (Unfortunately it also unnecessarily publishes
# all the other symbols in the dynamic symbol table.
#
if(ANDROID)
  target_link_options(mythfrontend PUBLIC "-rdynamic")
endif()
