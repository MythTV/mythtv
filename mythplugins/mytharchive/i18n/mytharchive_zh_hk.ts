<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_HK" sourcelanguage="en_US">
<context>
    <name>(ArchiveUtils)</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="52"/>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation>找不到 MythArchive 工作目錄。
有否設定正確路徑？</translation>
    </message>
</context>
<context>
    <name>(MythArchiveMain)</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="100"/>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation>發現鎖定檔案，但其進程(process)未有執行！
會移除過時之鎖定檔案。</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="211"/>
        <source>Last run did not create a playable DVD.</source>
        <translation>未能製作能播放之 DVD。</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="218"/>
        <source>Last run failed to create a DVD.</source>
        <translation>未能製作 DVD。</translation>
    </message>
</context>
<context>
    <name>ArchiveFileSelector</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="205"/>
        <source>Find File To Import</source>
        <translation>要匯入之檔案</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="265"/>
        <source>The selected item is not a valid archive file!</source>
        <translation>所選項目並非有效封存檔！</translation>
    </message>
</context>
<context>
    <name>ArchiveSettings</name>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="20"/>
        <source>MythArchive Temp Directory</source>
        <translation>MythArchive 暫存目錄</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="23"/>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation>MythArchive 暫時存放檔案之地點。要＊很多＊空間。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="35"/>
        <source>MythArchive Share Directory</source>
        <translation>MythArchive 共享目錄</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="38"/>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation>MythArchive 存放 script、簡介影片及佈景主題檔之地點</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="50"/>
        <source>Video format</source>
        <translation>視訊格式</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="55"/>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation>DVD 使用之視訊格式：PAL 或 NTSC。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="64"/>
        <source>File Selector Filter</source>
        <translation>篩選檔案選擇</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="67"/>
        <source>The file name filter to use in the file selector.</source>
        <translation>選擇檔案時要如何篩選。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="76"/>
        <source>Location of DVD</source>
        <translation>DVD 位置</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="79"/>
        <source>Which DVD drive to use when burning discs.</source>
        <translation>燒錄 DVD 要用哪個光碟機。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="89"/>
        <source>DVD Drive Write Speed</source>
        <translation>DVD 燒錄速度</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="92"/>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation>燒錄 DVD 時使用之速度。設為 0 讓 growisofs 自動選擇最快速度。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="103"/>
        <source>Command to play DVD</source>
        <translation>播放 DVD 之指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="106"/>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation>試播 DVD 之指令。「內置」會用 MythTV 內置播放器。%f 則會以建立 DVD 之路徑代替，如「xine -pfhq --no-splash dvd:/%f」。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="119"/>
        <source>Copy remote files</source>
        <translation>複製遠端檔案</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="122"/>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation>如設置，遠端之檔案會先複製至本機才處理。可加快處理速度並節省網絡資源</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="134"/>
        <source>Always Use Mythtranscode</source>
        <translation>一定用 Mythtranscode</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="137"/>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation>如設置，mpeg2 檔一定會經過 mythtranscode 以清掃錯誤，對解決聲音問題可能有用。如已設定「用 ProjectX」的話則無效。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="149"/>
        <source>Use ProjectX</source>
        <translation>用 ProjectX</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="152"/>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation>如設置，會以 ProjectX 而非 mythtranscode 和 mythreplex 清除廣告及分拆 mpeg2 檔。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="163"/>
        <source>Use FIFOs</source>
        <translation>用 FIFO</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="166"/>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not supported on Windows platform</source>
        <translation>Script 會以「先進先出(FIFO)」方式將 mplex 的輸出傳給 dvdauthor，而毋須製作中間檔案。此舉在多路傳輸(multiplex)作業時可節省時間及磁碟空間，但不支援 Windows 平台</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="179"/>
        <source>Add Subtitles</source>
        <translation>添加字幕</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="182"/>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation>此選項會為最終之 DVD 添加字幕。要先設置「用 ProjectX」。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="192"/>
        <source>Main Menu Aspect Ratio</source>
        <translation>主選單寬高比</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="194"/>
        <location filename="../mytharchive/archivesettings.cpp" line="210"/>
        <source>4:3</source>
        <comment>Aspect ratio</comment>
        <translation>4:3</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="195"/>
        <location filename="../mytharchive/archivesettings.cpp" line="211"/>
        <source>16:9</source>
        <comment>Aspect ratio</comment>
        <translation>16:9</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="199"/>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation>製作主選單使用之寬高比。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="208"/>
        <source>Chapter Menu Aspect Ratio</source>
        <translation>章節選單寬高比</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="212"/>
        <location filename="../mytharchive/archivesettings.cpp" line="221"/>
        <source>Video</source>
        <translation>影片</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="217"/>
        <source>Aspect ratio to use when creating the chapter menu. &apos;%1&apos; means use the same aspect ratio as the associated video.</source>
        <extracomment>%1 is the translation of the &quot;Video&quot; combo box choice</extracomment>
        <translation>製作章節選單時要用的寬高比。「%1」代表使用和相關影片一樣的寬高比。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="228"/>
        <source>Date format</source>
        <translation>日期格式</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="231"/>
        <source>Samples are shown using today&apos;s date.</source>
        <translation>會以今天日期舉例。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="237"/>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation>會以明天日期舉例。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="255"/>
        <source>Your preferred date format to use on DVD menus. %1</source>
        <extracomment>%1 gives additional info on the date used</extracomment>
        <translation>用於 DVD 選單的首選日期格式。%1</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="264"/>
        <source>Time format</source>
        <translation>時間格式</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="271"/>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation>DVD 選單要顯示之時間格式。除非選擇帶有「上午」或「下午」之格式，否則會以 24-小時格式顯示。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="282"/>
        <source>Default Encoder Profile</source>
        <translation>預設編碼設定組合</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="284"/>
        <source>HQ</source>
        <comment>Encoder profile</comment>
        <translation>HQ</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="285"/>
        <source>SP</source>
        <comment>Encoder profile</comment>
        <translation>SP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="286"/>
        <source>LP</source>
        <comment>Encoder profile</comment>
        <translation>LP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="287"/>
        <source>EP</source>
        <comment>Encoder profile</comment>
        <translation>EP</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="291"/>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation>如檔案要重新編碼時使用之預設編碼設定組合。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="300"/>
        <source>mplex Command</source>
        <translation>mplex 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="304"/>
        <source>Command to run mplex</source>
        <translation>執行 mplex 之指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="313"/>
        <source>dvdauthor command</source>
        <translation>dvdauthor 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="317"/>
        <source>Command to run dvdauthor.</source>
        <translation>執行 dvdauthor 之指令。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="326"/>
        <source>mkisofs command</source>
        <translation>mkisofs 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="330"/>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation>執行 mkisofs 之指令（用來製作 ISO 映像檔）</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="339"/>
        <source>growisofs command</source>
        <translation>growisofs 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="343"/>
        <source>Command to run growisofs. (Used to burn DVDs)</source>
        <translation>執行 growisofs 之指令。(用來燒錄 DVD)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="352"/>
        <source>M2VRequantiser command</source>
        <translation>M2VRequantiser 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="356"/>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation>執行 M2VRequantiser 之指令。可有可無 - 如無安裝 M2VRequantiser 則留空。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="366"/>
        <source>jpeg2yuv command</source>
        <translation>jpeg2yuv 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="370"/>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation>執行 jpeg2yuv 之指令。其為 mjpegtools 套件一部份</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="379"/>
        <source>spumux command</source>
        <translation>spumux 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="383"/>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation>執行 spumux 之指令。其為 dvdauthor 套件一部份</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="392"/>
        <source>mpeg2enc command</source>
        <translation>mpeg2enc 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="396"/>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation>執行 mpeg2enc 之指令。其為 mjpegtools 套件一部份</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="405"/>
        <source>projectx command</source>
        <translation>projectx 指令</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="409"/>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation>執行 ProjectX 之指令。取代 mythtranscode 和 mythreplex，用來清除廣告及分拆 mpeg 檔。</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="418"/>
        <source>MythArchive Settings</source>
        <translation>MythArchive 設定</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="442"/>
        <source>MythArchive External Commands</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="vanished">MythArchive 設定 (2)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archivesettings.cpp" line="434"/>
        <source>DVD Menu Settings</source>
        <translation>DVD 選單設定</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="vanished">MythArchive 外部指令 (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="vanished">MythArchive 外部指令 (2)</translation>
    </message>
</context>
<context>
    <name>BurnMenu</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1091"/>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation>無法燒錄 DVD。
未能製作 DVD。</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1097"/>
        <location filename="../mytharchive/mythburn.cpp" line="1109"/>
        <source>Burn DVD</source>
        <translation>燒錄 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1098"/>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation>
放入空白 DVD 然後選擇以下選項。</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1110"/>
        <source>Burn DVD Rewritable</source>
        <translation>燒錄可重寫式 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1111"/>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation>燒錄可重寫式 DVD (先清掃)</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="1165"/>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation>不能執行 mytharchivehelper 以燒錄 DVD。</translation>
    </message>
</context>
<context>
    <name>BurnThemeUI</name>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="5"/>
        <source>Has an intro and contains a main menu with 4 recordings per page. Does not have a chapter selection submenu.</source>
        <translation>有簡介，有主選單（每版四個錄影節目）。無章節選擇子選單。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="6"/>
        <source>Has an intro and contains a summary main menu with 10 recordings per page. Does not have a chapter selection submenu, recording titles, dates or category.</source>
        <translation>有簡介，有主選單（每版十個錄影節目）。無章節選擇子選單、節目標題、日期或分類。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="7"/>
        <source>Has an intro and contains a main menu with 6 recordings per page. Does not have a scene selection submenu.</source>
        <translation>有簡介，有主選單（每版六個錄影節目）。無場景選擇子選單。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="8"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording.</source>
        <translation>有簡介，有主選單（每版三個錄影節目）以及帶八個章節點之場景選擇子選單。每個節目前都有節目詳情。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="9"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. Shows a program details page before each recording. Uses animated thumb images.</source>
        <translation>有簡介，有主選單（每版三個錄影節目）以及帶八個章節點之場景選擇子選單。每個節目前都有節目詳情。縮圖會播放影片。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="10"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points.</source>
        <translation>有簡介，有主選單（每版三個錄影節目）以及帶八個章節點之場景選擇子選單。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="11"/>
        <source>Has an intro and contains a main menu with 3 recordings per page and a scene selection submenu with 8 chapters points. All the thumb images are animated.</source>
        <translation>有簡介，有主選單（每版三個錄影節目）以及帶八個章節點之場景選擇子選單。縮圖會播放影片。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="12"/>
        <source>Creates an auto play DVD with no menus. Shows an intro movie then for each title shows a details page followed by the video in sequence.</source>
        <translation>製作會自動播放但無選單之 DVD。首先顯示簡介影片、節目詳情，然後順序播放。</translation>
    </message>
    <message>
        <location filename="../mythburn/themes/burnthemestrings.h" line="13"/>
        <source>Creates an auto play DVD with no menus and no intro.</source>
        <translation>製作會自動播放但無選單之 DVD（無簡介）。</translation>
    </message>
</context>
<context>
    <name>DVDThemeSelector</name>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="201"/>
        <location filename="../mytharchive/themeselector.cpp" line="212"/>
        <source>No theme description file found!</source>
        <translation>找不到佈景主題說明檔！</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="225"/>
        <source>Empty theme description!</source>
        <translation>佈景主題說明無內容！</translation>
    </message>
    <message>
        <location filename="../mytharchive/themeselector.cpp" line="230"/>
        <source>Unable to open theme description file!</source>
        <translation>無法開啟佈景主題說明檔！</translation>
    </message>
</context>
<context>
    <name>ExportNative</name>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="198"/>
        <source>You need to add at least one item to archive!</source>
        <translation>至少要有一個項目才能進行封存！</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="343"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="350"/>
        <source>Remove Item</source>
        <translation>移除項目</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="442"/>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation>不能製作 DVD。執行 script 時發生錯誤</translation>
    </message>
    <message>
        <location filename="../mytharchive/exportnative.cpp" line="478"/>
        <source>You don&apos;t have any videos!</source>
        <translation>沒有影片！</translation>
    </message>
</context>
<context>
    <name>FileSelector</name>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="61"/>
        <source>Find File</source>
        <translation>找檔案</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="64"/>
        <source>Find Directory</source>
        <translation>找目錄</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="67"/>
        <source>Find Files</source>
        <translation>找檔案</translation>
    </message>
    <message>
        <location filename="../mytharchive/fileselector.cpp" line="280"/>
        <source>The selected item is not a directory!</source>
        <translation>所選項目並非目錄！</translation>
    </message>
</context>
<context>
    <name>ImportNative</name>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="389"/>
        <source>You need to select a valid channel id!</source>
        <translation>要選擇至少一個有效頻道ID！</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="420"/>
        <source>It was not possible to import the Archive.  An error occured when running &apos;mytharchivehelper&apos;</source>
        <translation>無法匯入封存資料。執行 &apos;mytharchivehelper&apos; 時發生問題</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="550"/>
        <source>Select a channel id</source>
        <translation>選擇頻道ID</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="576"/>
        <source>Select a channel number</source>
        <translation>選擇頻道編號</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="602"/>
        <source>Select a channel name</source>
        <translation>選擇頻道名稱</translation>
    </message>
    <message>
        <location filename="../mytharchive/importnative.cpp" line="628"/>
        <source>Select a Callsign</source>
        <translation>選擇稱呼</translation>
    </message>
</context>
<context>
    <name>LogViewer</name>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="75"/>
        <source>Cannot find any logs to show!</source>
        <translation>無可顯示之記錄！</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="189"/>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation>已要求停止在背景進行製作。
可能要幾分鐘。</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="328"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="336"/>
        <source>Don&apos;t Auto Update</source>
        <translation>不自動更新</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="338"/>
        <source>Auto Update</source>
        <translation>自動更新</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="340"/>
        <source>Show Progress Log</source>
        <translation>顯示進度記錄</translation>
    </message>
    <message>
        <location filename="../mytharchive/logviewer.cpp" line="341"/>
        <source>Show Full Log</source>
        <translation>顯示完整記錄</translation>
    </message>
</context>
<context>
    <name>MythBurn</name>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="317"/>
        <location filename="../mytharchive/mythburn.cpp" line="437"/>
        <source>Using Cut List</source>
        <translation>使用 Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="322"/>
        <location filename="../mytharchive/mythburn.cpp" line="442"/>
        <source>Not Using Cut List</source>
        <translation>不使用 Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="328"/>
        <location filename="../mytharchive/mythburn.cpp" line="448"/>
        <source>No Cut List</source>
        <translation>無 Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="339"/>
        <source>You need to add at least one item to archive!</source>
        <translation>至少要有一個項目才能進行封存！</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="385"/>
        <source>Retrieving File Information. Please Wait...</source>
        <translation>正提取檔案資料。請稍候...</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="451"/>
        <source>Encoder: </source>
        <translation>編碼器：</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="763"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="774"/>
        <source>Don&apos;t Use Cut List</source>
        <translation>不使用 Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="779"/>
        <source>Use Cut List</source>
        <translation>使用 Cut List</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="784"/>
        <source>Remove Item</source>
        <translation>移除項目</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="785"/>
        <source>Edit Details</source>
        <translation>編輯詳情</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="786"/>
        <source>Change Encoding Profile</source>
        <translation>更改編碼設定組合</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="787"/>
        <source>Edit Thumbnails</source>
        <translation>編輯縮圖</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="922"/>
        <source>It was not possible to create the DVD.  An error occured when running the scripts</source>
        <translation>無法製作 DVD。執行 script 時發生問題</translation>
    </message>
    <message>
        <location filename="../mytharchive/mythburn.cpp" line="964"/>
        <source>You don&apos;t have any videos!</source>
        <translation>沒有影片！</translation>
    </message>
</context>
<context>
    <name>MythControls</name>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="329"/>
        <source>Toggle use cut list state for selected program</source>
        <translation>為所選節目切換使用 cut list 狀態</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="332"/>
        <source>Create DVD</source>
        <translation>製作 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="334"/>
        <source>Create Archive</source>
        <translation>進行封存</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="336"/>
        <source>Import Archive</source>
        <translation>匯入封存資料</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="338"/>
        <source>View Archive Log</source>
        <translation>檢視封存記錄</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="340"/>
        <source>Play Created DVD</source>
        <translation>播放已製作之 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/mytharchive.cpp" line="342"/>
        <source>Burn DVD</source>
        <translation>燒錄 DVD</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>MythArchive Temp Directory</source>
        <translation type="obsolete">MythArchive 暫存目錄</translation>
    </message>
    <message>
        <source>Location where MythArchive should create its temporary work files. LOTS of free space required here.</source>
        <translation type="obsolete">MythArchive 暫時存放檔案之地點。要＊很多＊空間。</translation>
    </message>
    <message>
        <source>MythArchive Share Directory</source>
        <translation type="obsolete">MythArchive 共享目錄</translation>
    </message>
    <message>
        <source>Location where MythArchive stores its scripts, intro movies and theme files</source>
        <translation type="obsolete">MythArchive 存放 script、簡介影片及佈景主題檔之地點</translation>
    </message>
    <message>
        <source>Video format</source>
        <translation type="obsolete">視訊格式</translation>
    </message>
    <message>
        <source>Video format for DVD recordings, PAL or NTSC.</source>
        <translation type="obsolete">DVD 使用之視訊格式：PAL 或 NTSC。</translation>
    </message>
    <message>
        <source>File Selector Filter</source>
        <translation type="obsolete">篩選檔案選擇</translation>
    </message>
    <message>
        <source>The file name filter to use in the file selector.</source>
        <translation type="obsolete">選擇檔案時要如何篩選。</translation>
    </message>
    <message>
        <source>Location of DVD</source>
        <translation type="obsolete">DVD 位置</translation>
    </message>
    <message>
        <source>Which DVD drive to use when burning discs.</source>
        <translation type="obsolete">燒錄 DVD 要用哪個光碟機。</translation>
    </message>
    <message>
        <source>DVD Drive Write Speed</source>
        <translation type="obsolete">DVD 燒錄速度</translation>
    </message>
    <message>
        <source>This is the write speed to use when burning a DVD. Set to 0 to allow growisofs to choose the fastest available speed.</source>
        <translation type="obsolete">燒錄 DVD 時使用之速度。設為 0 讓 growisofs 自動選擇最快速度。</translation>
    </message>
    <message>
        <source>Command to play DVD</source>
        <translation type="obsolete">播放 DVD 之指令</translation>
    </message>
    <message>
        <source>Command to run when test playing a created DVD. &apos;Internal&apos; will use the internal MythTV player. %f will be replaced with the path to the created DVD structure eg. &apos;xine -pfhq --no-splash dvd:/%f&apos;.</source>
        <translation type="obsolete">試播 DVD 之指令。「內置」會用 MythTV 內置播放器。%f 則會以建立 DVD 之路徑代替，如「xine -pfhq --no-splash dvd:/%f」。</translation>
    </message>
    <message>
        <source>Copy remote files</source>
        <translation type="obsolete">複製遠端檔案</translation>
    </message>
    <message>
        <source>If set files on remote filesystems will be copied over to the local filesystem before processing. Speeds processing and reduces bandwidth on the network</source>
        <translation type="obsolete">如設置，遠端之檔案會先複製至本機才處理。可加快處理速度並節省網絡資源</translation>
    </message>
    <message>
        <source>Always Use Mythtranscode</source>
        <translation type="obsolete">一定用 Mythtranscode</translation>
    </message>
    <message>
        <source>If set mpeg2 files will always be passed though mythtranscode to clean up any errors. May help to fix some audio problems. Ignored if &apos;Use ProjectX&apos; is set.</source>
        <translation type="obsolete">如設置，mpeg2 檔一定會經過 mythtranscode 以清掃錯誤，對解決聲音問題可能有用。如已設定「用 ProjectX」的話則無效。</translation>
    </message>
    <message>
        <source>Use ProjectX</source>
        <translation type="obsolete">用 ProjectX</translation>
    </message>
    <message>
        <source>If set ProjectX will be used to cut commercials and split mpeg2 files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">如設置，會以 ProjectX 而非 mythtranscode 和 mythreplex 清除廣告及分拆 mpeg2 檔。</translation>
    </message>
    <message>
        <source>Use FIFOs</source>
        <translation type="obsolete">用 FIFO</translation>
    </message>
    <message>
        <source>The script will use FIFOs to pass the output of mplex into dvdauthor rather than creating intermediate files. Saves time and disk space during multiplex operations but not  supported on Windows platform</source>
        <translation type="obsolete">Script 會以「先進先出(FIFO)」方式將 mplex 之輸出傳遞給 dvdauthor 而不會製作中間檔。此舉可節省時間與硬碟空間，但不支援 Windows</translation>
    </message>
    <message>
        <source>Add Subtitles</source>
        <translation type="obsolete">添加字幕</translation>
    </message>
    <message>
        <source>If available this option will add subtitles to the final DVD. Requires &apos;Use ProjectX&apos; to be on.</source>
        <translation type="obsolete">此選項會為最終之 DVD 添加字幕。要先設置「用 ProjectX」。</translation>
    </message>
    <message>
        <source>Main Menu Aspect Ratio</source>
        <translation type="obsolete">主選單寬高比</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the main menu.</source>
        <translation type="obsolete">製作主選單使用之寬高比。</translation>
    </message>
    <message>
        <source>Chapter Menu Aspect Ratio</source>
        <translation type="obsolete">章節選單寬高比</translation>
    </message>
    <message>
        <source>Aspect ratio to use when creating the chapter menu. Video means use the same aspect ratio as the associated video.</source>
        <translation type="obsolete">製作章節選單使用之寬高比。與相關影片之寬高比相同。</translation>
    </message>
    <message>
        <source>Date format</source>
        <translation type="obsolete">日期格式</translation>
    </message>
    <message>
        <source>Samples are shown using today&apos;s date.</source>
        <translation type="obsolete">會以今天日期舉例。</translation>
    </message>
    <message>
        <source>Samples are shown using tomorrow&apos;s date.</source>
        <translation type="obsolete">會以明天日期舉例。</translation>
    </message>
    <message>
        <source>Your preferred date format to use on DVD menus.</source>
        <translation type="obsolete">DVD 選單要用之日期格式。</translation>
    </message>
    <message>
        <source>Time format</source>
        <translation type="obsolete">時間格式</translation>
    </message>
    <message>
        <source>Your preferred time format to display on DVD menus. You must choose a format with &quot;AM&quot; or &quot;PM&quot; in it, otherwise your time display will be 24-hour or &quot;military&quot; time.</source>
        <translation type="obsolete">DVD 選單要顯示之時間格式。除非選擇帶有「上午」或「下午」之格式，否則會以 24-小時格式顯示。</translation>
    </message>
    <message>
        <source>Default Encoder Profile</source>
        <translation type="obsolete">預設編碼設定組合</translation>
    </message>
    <message>
        <source>Default encoding profile to use if a file needs re-encoding.</source>
        <translation type="obsolete">如檔案要重新編碼時使用之預設編碼設定組合。</translation>
    </message>
    <message>
        <source>mplex Command</source>
        <translation type="obsolete">mplex 指令</translation>
    </message>
    <message>
        <source>Command to run mplex</source>
        <translation type="obsolete">執行 mplex 之指令</translation>
    </message>
    <message>
        <source>dvdauthor command</source>
        <translation type="obsolete">dvdauthor 指令</translation>
    </message>
    <message>
        <source>Command to run dvdauthor.</source>
        <translation type="obsolete">執行 dvdauthor 之指令。</translation>
    </message>
    <message>
        <source>mkisofs command</source>
        <translation type="obsolete">mkisofs 指令</translation>
    </message>
    <message>
        <source>Command to run mkisofs. (Used to create ISO images)</source>
        <translation type="obsolete">執行 mkisofs 之指令（用來製作 ISO 映像檔）</translation>
    </message>
    <message>
        <source>growisofs command</source>
        <translation type="obsolete">growisofs 指令</translation>
    </message>
    <message>
        <source>Command to run growisofs. (Used to burn DVD&apos;s)</source>
        <translation type="obsolete">執行 growisofs 之指令（用來燒錄 DVD）</translation>
    </message>
    <message>
        <source>M2VRequantiser command</source>
        <translation type="obsolete">M2VRequantiser 指令</translation>
    </message>
    <message>
        <source>Command to run M2VRequantiser. Optional - leave blank if you don&apos;t have M2VRequantiser installed.</source>
        <translation type="obsolete">執行 M2VRequantiser 之指令。可有可無 - 如無安裝 M2VRequantiser 則留空。</translation>
    </message>
    <message>
        <source>jpeg2yuv command</source>
        <translation type="obsolete">jpeg2yuv 指令</translation>
    </message>
    <message>
        <source>Command to run jpeg2yuv. Part of mjpegtools package</source>
        <translation type="obsolete">執行 jpeg2yuv 之指令。其為 mjpegtools 套件一部份</translation>
    </message>
    <message>
        <source>spumux command</source>
        <translation type="obsolete">spumux 指令</translation>
    </message>
    <message>
        <source>Command to run spumux. Part of dvdauthor package</source>
        <translation type="obsolete">執行 spumux 之指令。其為 dvdauthor 套件一部份</translation>
    </message>
    <message>
        <source>mpeg2enc command</source>
        <translation type="obsolete">mpeg2enc 指令</translation>
    </message>
    <message>
        <source>Command to run mpeg2enc. Part of mjpegtools package</source>
        <translation type="obsolete">執行 mpeg2enc 之指令。其為 mjpegtools 套件一部份</translation>
    </message>
    <message>
        <source>projectx command</source>
        <translation type="obsolete">projectx 指令</translation>
    </message>
    <message>
        <source>Command to run ProjectX. Will be used to cut commercials and split mpegs files instead of mythtranscode and mythreplex.</source>
        <translation type="obsolete">執行 ProjectX 之指令。取代 mythtranscode 和 mythreplex，用來清除廣告及分拆 mpeg 檔。</translation>
    </message>
    <message>
        <source>MythArchive Settings</source>
        <translation type="obsolete">MythArchive 設定</translation>
    </message>
    <message>
        <source>MythArchive Settings (2)</source>
        <translation type="obsolete">MythArchive 設定 (2)</translation>
    </message>
    <message>
        <source>DVD Menu Settings</source>
        <translation type="obsolete">DVD 選單設定</translation>
    </message>
    <message>
        <source>MythArchive External Commands (1)</source>
        <translation type="obsolete">MythArchive 外部指令 (1)</translation>
    </message>
    <message>
        <source>MythArchive External Commands (2)</source>
        <translation type="obsolete">MythArchive 外部指令 (2)</translation>
    </message>
    <message>
        <source>Cannot find the MythArchive work directory.
Have you set the correct path in the settings?</source>
        <translation type="obsolete">找不到 MythArchive 工作目錄。
有否設定正確路徑？</translation>
    </message>
    <message>
        <source>It was not possible to create the DVD. An error occured when running the scripts</source>
        <translation type="obsolete">不能製作 DVD。執行 script 時發生錯誤</translation>
    </message>
    <message>
        <source>Cannot find any logs to show!</source>
        <translation type="obsolete">無可顯示之記錄！</translation>
    </message>
    <message>
        <source>Background creation has been asked to stop.
This may take a few minutes.</source>
        <translation type="obsolete">已要求停止在背景進行製作。
可能要幾分鐘。</translation>
    </message>
    <message>
        <source>Found a lock file but the owning process isn&apos;t running!
Removing stale lock file.</source>
        <translation type="obsolete">找到一個鎖定檔案，但其行程已沒有運作！
移除過時之鎖定檔案。</translation>
    </message>
    <message>
        <source>Last run did not create a playable DVD.</source>
        <translation type="obsolete">上次未能製作能播放之 DVD。</translation>
    </message>
    <message>
        <source>Last run failed to create a DVD.</source>
        <translation type="obsolete">上次未能製作 DVD。</translation>
    </message>
    <message>
        <source>You don&apos;t have any videos!</source>
        <translation type="obsolete">沒有影片！</translation>
    </message>
    <message>
        <source>Cannot burn a DVD.
The last run failed to create a DVD.</source>
        <translation type="obsolete">無法燒錄 DVD。
上次未能製作 DVD。</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="obsolete">燒錄 DVD</translation>
    </message>
    <message>
        <source>
Place a blank DVD in the drive and select an option below.</source>
        <translation type="obsolete">
放入空白 DVD 然後選擇以下選項。</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable</source>
        <translation type="obsolete">燒錄可重寫式 DVD</translation>
    </message>
    <message>
        <source>Burn DVD Rewritable (Force Erase)</source>
        <translation type="obsolete">燒錄可重寫式 DVD（先清除）</translation>
    </message>
    <message>
        <source>It was not possible to run mytharchivehelper to burn the DVD.</source>
        <translation type="obsolete">不能執行 mytharchivehelper 以燒錄 DVD。</translation>
    </message>
</context>
<context>
    <name>RecordingSelector</name>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="89"/>
        <location filename="../mytharchive/recordingselector.cpp" line="374"/>
        <location filename="../mytharchive/recordingselector.cpp" line="479"/>
        <source>All Recordings</source>
        <translation>全部錄影</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="110"/>
        <source>Retrieving Recording List.
Please Wait...</source>
        <translation>正提取錄影清單。
請稍候...</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="134"/>
        <source>Either you don&apos;t have any recordings or no recordings are available locally!</source>
        <translation>一是根本沒有錄影，一是不在本機！</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="182"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="189"/>
        <source>Clear All</source>
        <translation>全部清除</translation>
    </message>
    <message>
        <location filename="../mytharchive/recordingselector.cpp" line="190"/>
        <source>Select All</source>
        <translation>全選</translation>
    </message>
</context>
<context>
    <name>SelectDestination</name>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="29"/>
        <source>Single Layer DVD</source>
        <translation>單層 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="30"/>
        <source>Single Layer DVD (4,482 MB)</source>
        <translation>單層 DVD (4,482 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="33"/>
        <source>Dual Layer DVD</source>
        <translation>雙層 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="34"/>
        <source>Dual Layer DVD (8,964 MB)</source>
        <translation>雙層 DVD (8,964 MB)</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="37"/>
        <source>DVD +/- RW</source>
        <translation>DVD +/- RW</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="38"/>
        <source>Rewritable DVD</source>
        <translation>可重寫式 DVD</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="41"/>
        <source>File</source>
        <translation>檔案</translation>
    </message>
    <message>
        <location filename="../mytharchive/archiveutil.cpp" line="42"/>
        <source>Any file accessable from your filesystem.</source>
        <translation>任何閣下檔案系統可存取之檔案。</translation>
    </message>
    <message>
        <location filename="../mytharchive/selectdestination.cpp" line="262"/>
        <location filename="../mytharchive/selectdestination.cpp" line="318"/>
        <source>Unknown</source>
        <translation>不詳</translation>
    </message>
</context>
<context>
    <name>ThemeUI</name>
    <message>
        <location filename="themestrings.h" line="21"/>
        <source>A high quality profile giving approx. 1 hour of video on a single layer DVD</source>
        <translation>「高質」設定讓單層 DVD 可有約一小時影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="23"/>
        <source>A standard play profile giving approx. 2 hour of video on a single layer DVD</source>
        <translation>「標準」設定讓單層 DVD 可有約二小時影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="22"/>
        <source>A long play profile giving approx. 4 hour of video on a single layer DVD</source>
        <translation>「延長」設定讓單層 DVD 可有約四小時影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="20"/>
        <source>A extended play profile giving approx. 6 hour of video on a single layer DVD</source>
        <translation>「超延長」設定讓單層 DVD 可有約六小時影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="221"/>
        <source>Select Destination</source>
        <translation>選擇目的地</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="131"/>
        <source>Find</source>
        <translation>找尋</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="138"/>
        <source>Free Space:</source>
        <translation>可用空間：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="160"/>
        <source>Make ISO Image</source>
        <translation>製作 ISO 映像檔</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="52"/>
        <source>Burn to DVD</source>
        <translation>燒錄至 DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="135"/>
        <source>Force Overwrite of DVD-RW Media</source>
        <translation>燒錄可重寫式 DVD-RW（先清除）</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="58"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="197"/>
        <source>Prev</source>
        <translation>上一步</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="173"/>
        <source>Next</source>
        <translation>下一步</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="225"/>
        <source>Select Recordings</source>
        <translation>選擇錄影</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="241"/>
        <source>Show Recordings</source>
        <translation>顯示錄影</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="180"/>
        <source>OK</source>
        <translation>好</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="123"/>
        <source>File Finder</source>
        <translation>找檔案</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="48"/>
        <source>Back</source>
        <translation>返回</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="6"/>
        <source>%SIZE% ~ %PROFILE%</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="8"/>
        <source>%date% / %profile%</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="9"/>
        <source>%size% (%profile%)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="13"/>
        <source>0.00Gb</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="17"/>
        <source>12.34GB</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="18"/>
        <source>&lt;-- Details</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="19"/>
        <source>&lt;-- Main Menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="26"/>
        <source>Add Recordings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="28"/>
        <source>Add Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="29"/>
        <source>Add a recording or video to archive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="30"/>
        <source>Add a recording, video or file to archive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="32"/>
        <source>Archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="33"/>
        <source>Archive Callsign:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="34"/>
        <source>Archive Chan ID:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="35"/>
        <source>Archive Chan No:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="36"/>
        <source>Archive Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="38"/>
        <source>Archive Item:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="40"/>
        <source>Archive Items to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="41"/>
        <source>Archive Log Viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="42"/>
        <source>Archive Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="43"/>
        <source>Archive Name:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="46"/>
        <source>Associate Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="47"/>
        <source>Associated Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="49"/>
        <source>Burn</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="50"/>
        <source>Burn Created DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="51"/>
        <source>Burn the last created archive to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="53"/>
        <source>Burn to DVD:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="54"/>
        <source>CUTLIST</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="55"/>
        <source>Callsign</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="57"/>
        <source>Callsign:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="59"/>
        <source>Cancel Job</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="61"/>
        <source>Chan. Id:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="63"/>
        <source>Change</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="65"/>
        <source>Channel ID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="67"/>
        <source>Channel ID:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="68"/>
        <source>Channel Name:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="69"/>
        <source>Channel No</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="71"/>
        <source>Channel Number:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="74"/>
        <source>Chapter Menu --&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="75"/>
        <source>Chapter Menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="76"/>
        <source>Chapters</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="82"/>
        <source>Create ISO Image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="83"/>
        <source>Create a</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="84"/>
        <source>Create a DVD of your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="85"/>
        <source>Current Destination:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="87"/>
        <source>Current Position:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="88"/>
        <source>Current Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="89"/>
        <source>Current selected item size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="90"/>
        <source>Current size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="91"/>
        <source>Current: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="92"/>
        <source>DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="93"/>
        <source>DVD Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="94"/>
        <source>DVD Menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="96"/>
        <source>Date (time)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="98"/>
        <source>Destination</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="99"/>
        <source>Destination Free Space:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="103"/>
        <source>Details:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="104"/>
        <source>Done</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="105"/>
        <source>Edit Archive item Information</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="106"/>
        <source>Edit Details</source>
        <translation type="unfinished">編輯詳情</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="107"/>
        <source>Edit Thumbnails</source>
        <translation type="unfinished">編輯縮圖</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="108"/>
        <source>Eject your</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="110"/>
        <source>Encode your video</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="111"/>
        <source>Encode your video to a file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="112"/>
        <source>Encoded Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="113"/>
        <source>Encoder Profile:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="114"/>
        <source>Encoding Profile</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="116"/>
        <source>Error: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="118"/>
        <source>Export</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="119"/>
        <source>Export your</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="120"/>
        <source>Export your videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="121"/>
        <source>File</source>
        <translation type="unfinished">檔案</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="122"/>
        <source>File Browser</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="125"/>
        <source>File browser</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="126"/>
        <source>File...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="127"/>
        <source>Filename:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="128"/>
        <source>Filesize</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="132"/>
        <source>Find Location</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="134"/>
        <source>First, select the thumb image you want to change from the overview. Second, press the &apos;tab&apos; key to move to the select thumb frame button to change the image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="136"/>
        <source>Force Overwrite of DVD-RW Media:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="139"/>
        <source>Home</source>
        <translation>家</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="140"/>
        <source>Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="142"/>
        <source>Import an</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="143"/>
        <source>Import recordings from a native archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="144"/>
        <source>Import videos from an Archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="146"/>
        <source>Intro --&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="147"/>
        <source>Intro:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="148"/>
        <source>Local Callsign:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="149"/>
        <source>Local Chan ID:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="150"/>
        <source>Local Chan No:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="152"/>
        <source>Local Name:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="153"/>
        <source>Log</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="155"/>
        <source>Log viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="156"/>
        <source>MENU changes focus. Numbers 0-9 jump to that thumb image.
When the preview image has focus, UP/DOWN changes the seek amount, LEFT/RIGHT jumps forward/backward by the seek amount, and SELECT chooses the current preview image for the selected thumb image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="158"/>
        <source>Main Menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="159"/>
        <source>Main menu</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="161"/>
        <source>Make ISO image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="162"/>
        <source>Make ISO image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="163"/>
        <source>Media</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="165"/>
        <source>Metadata</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="166"/>
        <source>Name</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="168"/>
        <source>Name:  %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="169"/>
        <source>Navigation</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="172"/>
        <source>New: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="174"/>
        <source>No files are selected for DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="177"/>
        <source>Not Applicable</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="178"/>
        <source>Not Available in this Theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="179"/>
        <source>Not available in this theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="184"/>
        <source>Original Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="186"/>
        <source>Overwrite DVD-RW Media:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="188"/>
        <source>Parental Level</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="189"/>
        <source>Parental Level:</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="191"/>
        <source>Parental Level: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="194"/>
        <source>Play the last created archive DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="195"/>
        <source>Position View:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="196"/>
        <source>Press &apos;left&apos; and &apos;right&apos; arrows to move through the frames. Press &apos;up&apos; and &apos;down&apos; arrows to change seek amount. Press &apos;select&apos; or &apos;enter&apos; key to lock the selected thumb image.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="198"/>
        <source>Preview</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="201"/>
        <source>Read video from data dvd or file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="203"/>
        <source>Recordings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="204"/>
        <source>Recordings group:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="206"/>
        <source>Save recordings and videos to a native archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="207"/>
        <source>Save recordings and videos to video DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="209"/>
        <source>Search Chan ID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="210"/>
        <source>Search Chan NO</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="212"/>
        <source>Search Local</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="216"/>
        <source>Seek Amount:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="217"/>
        <source>Seek amount:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="220"/>
        <source>Select Channel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="224"/>
        <source>Select Files</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="226"/>
        <source>Select Recordings for your archive or image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="227"/>
        <source>Select Videos</source>
        <translation>選擇影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="228"/>
        <source>Select a destination for your archive or image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="233"/>
        <source>Select thumb image:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="234"/>
        <source>Select your DVD menu theme</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="235"/>
        <source>Selected Items to be archived</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="236"/>
        <source>Selected Items to be burned</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="237"/>
        <source>Selected recording item size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="238"/>
        <source>Selected video item size: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="239"/>
        <source>Sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="240"/>
        <source>Show</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="242"/>
        <source>Show Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="243"/>
        <source>Show log with archive activities</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="244"/>
        <source>Show the Archive Log Viewer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="245"/>
        <source>Size:</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="246"/>
        <source>Size: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="249"/>
        <source>Start Time: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="250"/>
        <source>Start to</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="251"/>
        <source>Start to Burn your archive to DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="253"/>
        <source>Subtitle: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="254"/>
        <source>Test</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="255"/>
        <source>Test created</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="256"/>
        <source>Test created DVD</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="257"/>
        <source>Theme description</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="258"/>
        <source>Theme for the DVD menu:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="259"/>
        <source>Theme preview:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="264"/>
        <source>Title: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="265"/>
        <source>Tools and Utilities for archives</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="268"/>
        <source>Use archive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="269"/>
        <source>Used: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="270"/>
        <source>Utilities</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="271"/>
        <source>Utilities for MythArchive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="272"/>
        <source>Video Category</source>
        <translation>影片分類</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="293"/>
        <source>~</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="187"/>
        <source>PL:</source>
        <translation>家長分級：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="176"/>
        <source>No videos available</source>
        <translation>無影片提供</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="154"/>
        <source>Log Viewer</source>
        <translation>記錄檢視</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="267"/>
        <source>Update</source>
        <translation>更新</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="117"/>
        <source>Exit</source>
        <translation>結束</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="64"/>
        <source>Change Encoding Profile</source>
        <translation>更改編碼設定組合</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="95"/>
        <source>DVD Menu Theme</source>
        <translation>DVD 選單佈景主題</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="229"/>
        <source>Select a theme</source>
        <translation>選擇佈景主題</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="145"/>
        <source>Intro</source>
        <translation>簡介</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="157"/>
        <source>Main Menu</source>
        <translation>主選單</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="73"/>
        <source>Chapter Menu</source>
        <translation>章節選單</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="102"/>
        <source>Details</source>
        <translation>詳情</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="218"/>
        <source>Select Archive Items</source>
        <translation>選擇封存項目</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="175"/>
        <source>No files are selected for archive</source>
        <translation>未選擇要封存之影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="25"/>
        <source>Add Recording</source>
        <translation>添加錄影</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="27"/>
        <source>Add Video</source>
        <translation>添加影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="24"/>
        <source>Add File</source>
        <translation>添加檔案</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="11"/>
        <source>0 mb</source>
        <translation>0 mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="281"/>
        <source>frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="282"/>
        <source>increase seek amount</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="283"/>
        <source>move left</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="284"/>
        <source>move right</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location filename="themestrings.h" line="285"/>
        <source>profile: %n</source>
        <translation type="unfinished">
            <numerusform></numerusform>
        </translation>
    </message>
    <message>
        <location filename="themestrings.h" line="288"/>
        <source>to</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="292"/>
        <source>xxxxx mb</source>
        <translation>xxxxx mb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="37"/>
        <source>Archive Item Details</source>
        <translation>封存項目詳情</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="263"/>
        <source>Title:</source>
        <translation>節目名：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="252"/>
        <source>Subtitle:</source>
        <translation>副題：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="247"/>
        <source>Start Date:</source>
        <translation>開始日期：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="262"/>
        <source>Time:</source>
        <translation>時間：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="97"/>
        <source>Description:</source>
        <translation>說明：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="261"/>
        <source>Thumb Image Selector</source>
        <translation>縮圖選擇</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="86"/>
        <source>Current Position</source>
        <translation>當前位置</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="215"/>
        <source>Seek Amount</source>
        <translation>搜索時間</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="137"/>
        <source>Frame</source>
        <translation>格數</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="205"/>
        <source>Save</source>
        <translation>儲存</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="31"/>
        <source>Add video</source>
        <translation>添加影片</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="124"/>
        <source>File Finder To Import</source>
        <translation>找檔案匯入</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="248"/>
        <source>Start Time:</source>
        <translation>開始格式：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="219"/>
        <source>Select Associated Channel</source>
        <translation>選擇相關頻道</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="45"/>
        <source>Archived Channel</source>
        <translation>已封存頻道</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="60"/>
        <source>Chan. ID:</source>
        <translation>頻道ID：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="62"/>
        <source>Chan. No:</source>
        <translation>頻道編號：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="56"/>
        <source>Callsign:</source>
        <translation>稱呼：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="167"/>
        <source>Name:</source>
        <translation>名稱：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="151"/>
        <source>Local Channel</source>
        <translation>本地頻道</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="211"/>
        <source>Search Channel</source>
        <translation>搜尋頻道</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="208"/>
        <source>Search Callsign</source>
        <translation>搜尋稱呼</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="213"/>
        <source>Search Name</source>
        <translation>搜尋名稱</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="133"/>
        <source>Finish</source>
        <translation>完結</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="5"/>
        <source>%DATE%, %TIME%</source>
        <translation>%DATE%  %TIME%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="78"/>
        <source>Choose where you would like your files archived.</source>
        <translation>選擇檔案封存目的地。</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="185"/>
        <source>Output Type:</source>
        <translation>輸出類型：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="100"/>
        <source>Destination:</source>
        <translation>目的地：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="79"/>
        <source>Click here to find an output location...</source>
        <translation>點此尋找輸出地點...</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="115"/>
        <source>Erase DVD-RW before burning</source>
        <translation>燒錄 DVD-RW 前先清除</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="199"/>
        <source>Previous</source>
        <translation>上一個</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="130"/>
        <source>Filter:</source>
        <translation>篩選：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="231"/>
        <source>Select the file you wish to use.</source>
        <translation>選擇要用之檔案。</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="214"/>
        <source>See logs from your archive runs.</source>
        <translation>執行封存時觀看詳情。</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="16"/>
        <source>12.34 GB</source>
        <translation>12.34 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="77"/>
        <source>Choose the appearance of your DVD.</source>
        <translation>選擇 DVD 外觀。</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="260"/>
        <source>Theme:</source>
        <translation>佈景主題：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="232"/>
        <source>Select the recordings and videos you wish to save.</source>
        <translation>選擇要儲存之錄影及影片。</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="14"/>
        <source>0:00:00.00</source>
        <translation>0:00:00.00</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="266"/>
        <source>Up Level</source>
        <translation>上一層</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="274"/>
        <source>Video category:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="275"/>
        <source>Videos</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="276"/>
        <source>View progress of your archive or image</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="277"/>
        <source>Write video to data dvd or file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="278"/>
        <source>XML File to Import</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="279"/>
        <source>decrease seek amount</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="themestrings.h" line="280"/>
        <source>description goes here.</source>
        <translation>在此輸入說明。</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="12"/>
        <source>0.00 GB</source>
        <translation>0.00 GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="273"/>
        <source>Video Category:</source>
        <translation>影片分類：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="287"/>
        <source>title goes here</source>
        <translation>在此輸入節目名</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="15"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="291"/>
        <source>xxxxx MB</source>
        <translation>xxxxx MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="10"/>
        <source>0 MB</source>
        <translation>0 MB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="286"/>
        <source>sep 13, 2004 11:00 pm (1h 15m)</source>
        <translation>2004年9月13日  下午 11:00  (1小時15分)</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="289"/>
        <source>x.xx GB</source>
        <translation>x.xx GB</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="181"/>
        <source>Ok</source>
        <translation>好</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="129"/>
        <source>Filesize: %1</source>
        <translation>檔案大小：%1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="202"/>
        <source>Recorded Time: %1</source>
        <translation>已紀錄時間：%1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="7"/>
        <source>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</source>
        <translation>%&quot;|SUBTITLE|&quot; %%(|ORIGINALAIRDATE|) %%(|STARS|) %%DESCRIPTION%</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="190"/>
        <source>Parental Level: %1</source>
        <translation>家長分級：%1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="39"/>
        <source>Archive Items</source>
        <translation>封存項目</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="200"/>
        <source>Profile:</source>
        <translation>設定組合：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="182"/>
        <source>Old Size:</source>
        <translation>舊的大小：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="170"/>
        <source>New Size:</source>
        <translation>新的大小：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="290"/>
        <source>x.xx Gb</source>
        <translation>x.xx Gb</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="222"/>
        <source>Select Destination:</source>
        <translation>選擇目的地：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="192"/>
        <source>Parental level: %1</source>
        <translation>家長分級：%1</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="183"/>
        <source>Old size:</source>
        <translation>舊大小：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="171"/>
        <source>New size:</source>
        <translation>新大小：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="230"/>
        <source>Select a theme:</source>
        <translation>選擇佈景主題：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="164"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="72"/>
        <source>Chapter</source>
        <translation>章節</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="101"/>
        <source>Detail</source>
        <translation>詳情</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="223"/>
        <source>Select File to Import</source>
        <translation>選擇要匯入之檔案</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="66"/>
        <source>Channel ID:</source>
        <translation>頻道ID：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="70"/>
        <source>Channel Number:</source>
        <translation>頻道編號：</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="81"/>
        <source>Create DVD</source>
        <translation>製作 DVD</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="80"/>
        <source>Create Archive</source>
        <translation>進行封存</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="109"/>
        <source>Encode Video File</source>
        <translation>為影片檔編碼</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="141"/>
        <source>Import Archive</source>
        <translation>匯入封存資料</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="44"/>
        <source>Archive Utilities</source>
        <translation>封存工具</translation>
    </message>
    <message>
        <source>Show Log Viewer</source>
        <translation type="vanished">顯示記錄檢視</translation>
    </message>
    <message>
        <location filename="themestrings.h" line="193"/>
        <source>Play Created DVD</source>
        <translation>播放已製作之 DVD</translation>
    </message>
    <message>
        <source>Burn DVD</source>
        <translation type="vanished">燒錄 DVD</translation>
    </message>
</context>
<context>
    <name>ThumbFinder</name>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="851"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="858"/>
        <source>Exit, Save Thumbnails</source>
        <translation>結束，並儲存縮圖</translation>
    </message>
    <message>
        <location filename="../mytharchive/thumbfinder.cpp" line="859"/>
        <source>Exit, Don&apos;t Save Thumbnails</source>
        <translation>結束，不儲存縮圖</translation>
    </message>
</context>
<context>
    <name>VideoSelector</name>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="141"/>
        <source>Menu</source>
        <translation>選單</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="148"/>
        <source>Clear All</source>
        <translation>全部清除</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="149"/>
        <source>Select All</source>
        <translation>全選</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="331"/>
        <location filename="../mytharchive/videoselector.cpp" line="497"/>
        <source>All Videos</source>
        <translation>全部影片</translation>
    </message>
    <message>
        <location filename="../mytharchive/videoselector.cpp" line="549"/>
        <source>You need to enter a valid password for this parental level</source>
        <translation>要輸入密碼才能進入此家長分級</translation>
    </message>
</context>
</TS>
