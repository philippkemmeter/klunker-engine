<?
@include_once (dirname(__FILE__).'/../__private_config.inc');

/* Wrapper-Funktion. Damit man weiterhin nur query eingeben kann. Dies wird dann auf $pv_db::query() gelinkt
 * mit globalem $pv_db.
 */

function query($sql_code) {
	global $pv_db, $con;
	if (!$pv_db) {
		$con = connect_db();
	}

	return $pv_db->query($sql_code);
}

/* Wrapper-Funktion. Damit man weiterhin nur $con=connect_db() eingeben kann. Das wird dann umgeleitet
 * etc :)
 */
function connect_db() {
	global $pv_db;
//	print_r(debug_backtrace());
	$pv_db = new Db();
	return $pv_db->getConId();
}

class Db {
	const LOGS_HOST = 'localhost';
	const LOGS_NAME = 'logs';
	const LOGS_SELECT_DB = 'logs';
	const LOGS_PW = 'Fpa9_Isj#98';

	const PAYMENT_HOST = 'localhost';
	const PAYMENT_NAME = 'payment';
	const PAYMENT_SELECT_DB = 'payment';
	const PAYMENT_PW = 'ja8s+74u8Q';

	const USER_PERSONAL_HOST = 'localhost';
	const USER_PERSONAL_SELECT_DB = 'user_personal';

	private $con_id;
	private $res;
	private $host;
	private $name;
	private $pw;
	private $select_db;
	private $persistant;

	private $querycounter = 1;

	const DEBUG = 0;

	public function __construct($host = self::PAYMENT_HOST,
		$name = self::PAYMENT_NAME, $pw = self::PAYMENT_PW,
		$select_db = self::PAYMENT_SELECT_DB, $persistant = false,
		$suppress_errors = false)
	{
		if (defined('TEST_SERVER')) {	// Datenbankwechsel auf testserver. Hier an zentraler Stelle, dass auch ja nix passiert :)
			$select_db .= '_test';
		}
		$this->con_id = mysql_connect($host, $name, $pw);

		if ((!$this->con_id || !@ mysql_select_db($select_db, $this->con_id))
				 && !$suppress_errors)
			die('<html><head><title>' .
			LNG_DB_ERROR . '</title></head><body><center><font color=red>' . LNG_ERR_NO_DB_CON . '</font></center></body></html>');
		else if (self::DEBUG)
			error_log("DB connected to $select_db.");

		$this->host = $host;
		$this->name = $name;
		$this->pw = $pw;
		$this->select_db = $select_db;
		$this->persistant = $persistant;
	}

	public function __destruct() {
		@ mysql_free_result($this->res);
		@ mysql_close();
	}

	public function query($sql_code) {
		$t = microtime(true);
		$this->res = mysql_query($sql_code, $this->con_id);
		$took = (microtime(true) - $t);
		if (self::DEBUG) {
			error_log("sql-query" . $this->querycounter++ . ": $sql_code ".
				"(Duration: $took)");
		}
		if (!$this->res) {
			error_log("A database error occured. The crazy code was:
						$sql_code

						MySQL error text: " . mysql_error(). "\n"/*.var_export(debug_backtrace(),true)*/,
						'Unknown', 'DB query error');
			throw new Exception("Database Error! Time:".date("Y-m-d, H:i:s",Time::get_t_now()));
		}
		return $this->res;
	}

	public function getConId() {
		return $this->con_id;
	}
	public function getHost() {
		return $this->host;
	}
	public function getName() {
		return $this->name;
	}
	public function getPW() {
		return $this->pw;
	}
	public function getSelectDb() {
		return $this->select_db;
	}
	public function getPersistant() {
		return $this->persistant;
	}
}

?>