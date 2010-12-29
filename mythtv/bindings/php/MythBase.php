<?php
/**
 *  This is the base object to handle all the common features for all myth classes
 *
 * @license     GPL
 *
 * @package     MythWeb
 * @subpackage  Classes
 *
/**/

class MythBase {
    public $cacheKey = null;
    public $cacheLifetime = 600;
    private static $instances = array();

    public static function &find() {
        $class = get_called_class();
        $args = func_get_args();
        $key = self::getCacheKey($class, $args);
        if (is_null($key)) {
            $r = new ReflectionClass($class);
            $obj = $r->newInstanceArgs($args);
            return $obj;
        }
        elseif (!isset(self::$instances[$key])) {
            self::$instances[$key] = &Cache::getObject($key);
            if (is_null(self::$instances[$key]) || !is_object(self::$instances[$key]) || get_class(self::$instances[$key]) !== $class) {
                $r = new ReflectionClass($class);
                self::$instances[$key] = $r->newInstanceArgs($args);
                self::$instances[$key]->cacheKey = $key;
            }
        }
        return self::$instances[$key];
    }

    public function clearCache() {
        if (!is_null($this->cacheKey))
            return Cache::delete($this->cacheKey);

        $key = $this->getCacheKey();
        if (!is_null($key))
            return Cache::delete($key);

        return false;
    }

    public function __destruct() {
        if (!is_null($this->cacheKey))
            Cache::setObject($this->cacheKey, &$this, $this->cacheLifetime);
        $this->cacheKey = null;
    }

    public function __wakeup() {
        if (!is_null($this->cacheKey)) {
        // Prevent multiple cache writes for a single key
            if (isset(self::$instances[$this->cacheKey]))
                self::$instances[$this->cacheKey]->cacheKey = null;
            self::$instances[$this->cacheKey] = &$this;
        }
    }

    private static function getCacheKey(&$class = null, &$data = null) {
        if (is_null($class)) {
            $class = get_called_class();
            $data = &$this;
        }
        elseif (is_object($class)) {
            $data =& $class;
            $class = get_class($data);
        }

        switch ($class) {
            case 'Program':
                if (isset($data[0]['chanid']))
                    $ret = implode(',', array('chanid'=>$data[0]['chanid'], 'starttime'=>$data[0]['starttime']));
                elseif (isset($data['chanid']))
                    $ret = implode(',', array('chanid'=>$data['chanid'], 'starttime'=>$data['starttime']));
                elseif (isset($data[4]) && isset($data[10]))
                    $ret = implode(',', array('chanid'=>$data['4'], 'starttime'=>$data['10']));
                else
                    return null;
                break;
            case 'Channel':
                if (isset($data['chanid']))
                    $ret = $data['chanid'];
                elseif (isset($data[0]))
                    $ret = $data[0];
                else
                    return null;
                break;
            case 'Translate':
                $ret = '';
                break;
            case 'Schedule':
                if (is_null($data) || is_object($data))
                    return null;
                $ret = implode(',', $data);
                break;
            default:
                $ret = implode(',', $data);
                break;
        }
        return "$class($ret)";
    }

}
