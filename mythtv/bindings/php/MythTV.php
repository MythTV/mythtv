<?php

class MythTV {
    public $VERSION            = '.25pre';

    public $RecordingTypes     = array( 1  => 'Once',
                                        2  => 'Daily',
                                        3  => 'Channel',
                                        4  => 'Always',
                                        5  => 'Weekly',
                                        6  => 'Find Once',
                                        7  => 'Override',
                                        8  => 'Dont Record',
                                        9  => 'Find Daily',
                                        10 => 'Find Weekly' );

    public $Searches           = array( 1  => 'Power',
                                        2  => 'Title',
                                        3  => 'Keyword',
                                        4  => 'People',
                                        5  => 'Manual' );

# Reasons a recording wouldn't be happening (from libs/libmythtv/programinfo.h)
    public $RecordingStatus     = array(-8 => 'TunerBusy',
                                        -7 => 'LowDiskSpace',
                                        -6 => 'Cancelled',
                                        -5 => 'Deleted',
                                        -4 => 'Aborted',
                                        -3 => 'Recorded',
                                        -2 => 'Recording',
                                        -1 => 'WillRecord',
                                         0 => 'Unknown',
                                         1 => 'DontRecord',
                                         2 => 'PreviousRecording',
                                         3 => 'CurrentRecording',
                                         4 => 'EarlierShowing',
                                         5 => 'TooManyRecordings',
                                         6 => 'NotListed',
                                         7 => 'Conflict',
                                         8 => 'LaterShowing',
                                         9 => 'Repeat',
                                        10 => 'Inactive',
                                        11 => 'NeverRecord' );

// List of the default locations of the config files and the one we are using
    public $ConfigFiles        = array( '/usr/local/share/mythtv/mysql.txt',
                                        '/usr/share/mythtv/mysql.txt',
                                        '/usr/local/etc/mythtv/mysql.txt',
                                        '/etc/mythtv/mysql.txt',
                                        '~/.mythtv/mysql.txt',
                                        'mysql.txt' );
    public $ConfigFile         = NULL;
// This stores all the config file publics
    public $Config             = array();
// Database Object
    public $DB                 = NULL;
// Open Sockets
    public $SOCKETS            = array();
//
    public $Channels           = array();
    public $Scheduled          = array();
    public $Recorded           = array();

    public $Host                = NULL;

    private $MythProto          = NULL;

    public function __construct($host = NULL) {
        if (is_null($host))
            $this->Host = $this->Setting('master_host');
        else
            $this->Host = $host;
        $this->MythProto = new MythProto($this);

        if (isset($_ENV['MYTHCONFDIR']))
            $this->ConfigFiles[] = $_ENV['MYTHCONFDIR'];
    // look for config files
        $found = FALSE;
        foreach ($this->ConfigFiles as $ConfigFile) {
        // Php does not appear to honor ~, so we fake it.
            $ConfigFile = str_replace('~', $_ENV['HOME'], $ConfigFile);
            $this->ConfigFile = realpath($ConfigFile);
            if ($this->ConfigFile) {
                $found = TRUE;
                break;
            }
        }
        if (!$found)
            die("Cannot find a valid config file.\n");
        $this->LoadConfig();
        $this->DB = Database::connect( $this->Config['DBName'],
                                       $this->Config['DBUserName'],
                                       $this->Config['DBPassword'],
                                       $this->Config['DBHostName'],
                                       null,
                                       'mysql' );
    // Unset the password to protect the mysql server
        unset($this->Config['DBPassword']);
    }

// We use the $this->ConfigFile public to load from
    private function LoadConfig() {
        $file = fopen($this->ConfigFile, 'r');
        while (!feof($file)) {
            $line = trim(fgets($file));
        // skip comments and empty lines!
            if (strlen($line) === 0 || strpos($line, '#') === 0)
                continue;
            list ($public, $value) = explode('=', $line, 2);
            $this->Config[$public] = $value;
        }
        fclose($file);
    }

    public function Setting($name, $hostname = NULL, $value = "old\0old") {
        static $CACHE;
        if ($value !== "old\0old") {
            if (is_null($hostname))
                $this->DB->query('DELETE FROM settings WHERE settings.value = ? AND settings.hostname IS NULL', $name);
            else
                $this->DB->query('DELETE FROM settings WHERE settings.value = ? AND settings.hostname = ?', $name, $hostname);
            $this->DB->query('INSERT INTO settings (value, hostname, data) VALUES ( ?, ?, ?)', $name, $hostname, $value);
            $CACHE[$hostname][$name] = $value;
        }
        elseif (!isset($CACHE[$hostname][$name])) {
            if (is_null($hostname))
                $CACHE[$hostname][$name] = $this->DB->query_col('SELECT settings.data FROM settings WHERE settings.value = ? AND settings.hostname IS NULL', $name);
            else
                $CACHE[$hostname][$name] = $this->DB->query_col('SELECT settings.data FROM settings WHERE settings.value = ? AND settings.hostname = ?', $name, $hostname);
        }
        return $CACHE[$hostname][$name];
    }

    public function Command($command, $host = NULL, $port = NULL, $sendonly = FALSE) {
        $this->Send($command, $host, $port);
        if (!$sendonly)
            return $this->Receive($host);
    }

    public function Send($command, $host, $port) {
        if (is_null($host))
            $host = $this->Setting('master_host');
        if (is_null($port))
            $port = $this->Setting('master_port');
        if (!isset($this->SOCKETS[$host])) {
             $this->SOCKETS[$host] = $this->CreateSocket($host, $port);
        // Check protocol versions for each new connection
            $proto_ver = $this->Command('MYTH_PROTO_VERSION', $host, $port);
            if ($this->PROTO_VER != $proto_ver)
                die("$host is proto version $proto_ver, but we expect $this->PROTO_VER\n");
        }
        $this->SendData($this->SOCKETS[$host], $command);
    }

    public function SendData($socket, $data = NULL) {
        if (is_null($data))
            return FALSE;
        if (is_array($data))
            $data = implode($this->BACKEND_SEP, $data);
        if (fwrite($socket, str_pad(strlen($data), 8, ' ', STR_PAD_RIGHT).$data) === FALSE)
            die("Failure sending $data\n");
    }

    public function CreateSocket($host, $port, $data = NULL) {
        $trys = 5;
        do {
            $socket = fsockopen( $host,
                                 $port,
                                 $errno,
                                 $errstr,
                                 25 );
        } while ($errno != 0 && $trys-- > 0);
        if ($trys == 0)
            die("Can't connect to $host:$port [$errno] $errstr\n");
        $this->SendData($socket, $data);
        return $socket;
    }

    public function Receive($host) {
        $data = '';
        $length = intval(fread($this->SOCKETS[$host], 8));
        do {
            $data .= fread($this->SOCKETS[$host], ($length - strlen($data)));
        } while (strlen($data) < $length);
        return $data;
    }

    public function StreamBackendFile($basename, $fh, $local_path = NULL, $host = NULL, $port = NULL, $seek = 0) {
        if (is_null($host))
            $host = $this->Setting('master_host');
        if (is_null($port))
            $port = $this->Setting('master_port');
        if (!is_resource($fh)) {
            $target_path = $fh;
            $fh = fopen($target_path, 'w+');
        }
        if (!is_resource($fh))
            die("Can't create file $target_path or write to fh $fh\n");
        if (!is_null($local_path) && file_exists($local_path)) {
            $infh = fopen($local_path, 'r');
            if (is_resource($infh)) {
                fseek($infh, $seek);
                while(!feof($infh))
                    fwrite($fh, fread($infh, 2097152));
                fclose($fh);
                fclose($infh);
                return TRUE;
            }
            else
                fwrite(STDERR, "Can't read $local_path.  Trying to stream from mythbackend instead.\n");
        }
        return FALSE;
    }

    public function BackendRows($command, $offset = 0) {
        $data = explode($this->BACKEND_SEP, $this->Command($command));
        $rows = array();
        $row  = 0;
        $col  = 0;
        foreach ($data as $entry) {
            $rows[$row][$col] = $entry;
            $col++;
            if ($col >= $this->NUMPROGRAMLINES) {
                $rows++;
                $col = 0;
            }
        }
    // Offset data, if any
        for ($i = 0; $i < $offset; $i++)
            $rows['offset'][$i] = $data[$i];
        return $rows;
    }

    public function StatusCommand($path = '', $params = array(), $host = NULL, $port = NULL) {
        if (is_null($host))
            $host = $this->Setting('master_host');
        if (is_null($port))
            $port = $this->Setting('BackendStatusPort');
        $query = '';
        if (is_array($params) && count($params)) {
            foreach($params as $key => $value) {
                if (strlen($query))
                    $query .= '&';
                $query.=$key.'='.urlencode($value);
            }
        }
        $socket = fsockopen($host, $port, $errno, $errstr);
        if (!$socket)
            die("Error connecting to $host:$port/$path to send status command\n");
        fwrite($socket, "POST /$path HTTP/1.1\n");
        fwrite($socket, "Host: $host\n");
        fwrite($socket, "Content-Type: application/x-www-form-urlencoded\n");
        fwrite($socket, 'Content-Length: '.strlen($query)."\n");
        fwrite($socket, "\n");
        fwrite($socket, "$query\n");
        while (!feof($socket))
            $result .= fread($socket);
        fclose($socket);
        return $result;
    }

    public function Channel($ChanID) {
        if (!isset($this->Channels[$ChanID]))
            $this->LoadChannels();
        return $this->Channels[$ChanID];
    }

    public function LoadChannels() {
        $channelids = $sh->query('SELECT channel.chanid
                                    FROM channel');
        while ($chanid = $channelids->fetch_col())
            if (!isset($this->Channels[$chanid]))
                $this->Channels[$chanid] = new MythTVChannel($this, $chanid);
    }

    public function UnixToMyth($timestamp) {
        if (!is_numeric($timestamp))
            $timestamp = strtotime($timestamp);
        return date('Y-m-d H:i:s', $timestamp);
    }

    public function MythToUnix($time) {
        return strtotime($time);
    }

    public function Recording($chanid = NULL, $starttime = NULL, $basename = NULL) {
        return FALSE;
    }

    public function GetRecordingDirs() {
        $dirs = array();
        $sh = $this->DB->query('SELECT DISTINCT storagegroup.dirname
                                           FROM storagegroup');
        while ($dir = $sh->fetch_col())
            $dirs[] = trim($dir);
        return $dirs;
    }

    public function GetLocalHostname() {
        static $hostname = NULL;
        if (!is_null($hostname))
            $hostname = `hostname`;
        return $hostname;
    }
}
