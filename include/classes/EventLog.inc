<?php
require_once(dirname(__FILE__).'/IEventLog.inc');
require_once(dirname(__FILE__).
				'/../shared/classes/exceptions/IllegalArgumentException.inc');

/**
 * Diese Klasse ist verantwortlich für einen Log, der pro Spieler pro Karte die
 * Ereignisse auf der Karte protokolliert (Schätze verkauft, bekommen usw.).
 *
 * @author Philipp Kemmeter
 */
class EventLog implements IEventLog {
	private $uid;
	private $data_to_insert = false;


	/**
	 * @var EventLog
	 */
	private static $INSTANCES;

	private static $db;

	/**
	 * @param uint $uid
	 * @return EventLog
	 */
	public static function get_instance($uid) {
		$uid = (int)$uid;
		if (!isset(self::$INSTANCES[$uid]))
			self::$INSTANCES[$uid] = new self($uid);
		return self::$INSTANCES[$uid];
	}

	/**
	 * Gibt die aktuelle Datenbank zurück, in welcher die Punkte gespeichert
	 * werden/sind.
	 *
	 * @return Db
	 */
	private static function get_db() {
		if (!self::$db)
			self::$db = new Db(Db::LOGS_HOST, Db::LOGS_NAME,
								Db::LOGS_PW, Db::LOGS_SELECT_DB);
		return self::$db;
	}

	private function __construct($uid) {
		$this->uid = $uid;
	}


	public function __destruct() {
		if (defined('TEST_SERVER'))
			return;
		if ($this->data_to_insert) {
			/**
			 * Alle Daten einfügen. Idee der Query: Wenn ein Datum extakt
			 * dasselbe mal auftritt, dann soll es keine neue Zeile in der
			 * Tabelle beanspruchen, sondern es soll ein Counter erhöht werden
			 * "num". So bleibt die Ausgabe übersichtlich, da es sich
			 * wahrscheinlich um einen Fehler handelt. Der Fehler kann aber
			 * so getrackt werden (im Gegensatz zu INSERT IGNORE...).
			 */
			$n = count($this->data_to_insert);
			$sql =
				"INSERT INTO game_event_log_no_map ".
					"(uid, type, extra, extra_str, t_stamp, num) ".
				"VALUES ";
			$first = true;
			for ($i = 0; $i < $n; $i++) {
				if ($first)
					$first = false;
				else
					$sql .= ',';

				$sql .=
					"({$this->uid}, ".
					"{$this->data_to_insert[$i]->type}, ".
					"{$this->data_to_insert[$i]->extra}, ".
					"'{$this->data_to_insert[$i]->extra_str}', ".
					"{$this->data_to_insert[$i]->t_stamp}, ".
					"1)";

			}
			$sql .=
				" ON DUPLICATE KEY UPDATE num=num+1";
			self::get_db()->query($sql);
		}
	}

	/**
	 * Fügt einen Eintrag in den Log ein. Wird $t_stamp nicht gesetzt, wird
	 * JETZT als Zeitpunkt gewählt. Ansonsten können frei Zeitpunkte (auch
	 * vergangene oder zukünftige) gewählt werden.
	 *
	 * @param uint $type
	 * @param uint $extra
	 * @param uint $t_stamp
	 */
	public function add_entry($type, $extra, $extra_str, $t_stamp=0) {
		if (($type < self::TYPE_PAYMENT_MIN)||($type > self::TYPE_PAYMENT_MAX)){
			throw new IllegalArgumentException("type",
				'['.self::TYPE_PAYMENT_MIN.';'.self::TYPE_PAYMENT_MAX.']',
				$type);
		}
		$type = (int)$type;

		if ($t_stamp <= 0)
			$t_stamp = Time::get_t_now();

		/**
		 * Einzufügendes Datum erzeugen.
		 */
		$row = new stdClass();
		$row->extra = (int)$extra;
		$row->extra_str = $extra_str;
		$row->t_stamp = (int)$t_stamp;
		if ($this->log !== false) {
			/**
			 * Wenn aus der DB ein Log bereits ausgelesen wurde, diesen dann
			 * aktualisieren (TODO: kann zu Dopplungen führen)
			 */
			$this->log[$type][] = $row;
			$this->log[$type][count($this->log[$type])-1]->num = 1;
		}

		/**
		 * Das Datum vermerken, um es bei Laufzeitende in die DB zu schreiben.
		 */
		$this->data_to_insert[] = $row;
		$this->data_to_insert[count($this->data_to_insert)-1]->type = $type;
	}
}

?>