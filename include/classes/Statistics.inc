<?
require_once(dirname(__FILE__).'/Db.inc');
/**
 * Diese Klasse macht statistische Abfragen.
 */

class Statistics {
	private function __construct() {}	// rein statische Klasse


	/**
	 * Gibt die Anzahl aller Items zurück, die User benutzen, gruppiert
	 * nach Item Type-ID.
	 *
	 * Mithilfe von <code>$user_ids</code> kann ein Filter eingestellt werden,
	 * um nur Items dieser User zu berücksichtigen.
	 *
	 * @param uint[] $user_ids
	 * @return uint[][]
	 */
	public static function get_all_user_items_amount(array $user_ids=null) {
		$sql =
			"SELECT type, id, SUM(amount_mp)+SUM(amount) AS amount ".
			"FROM user_items ";
		if (!empty($user_ids)) {
			$sql .= "WHERE uid IN (".implode(',', $user_ids).") ";
		}
		$sql .= "GROUP BY type, id";

		$res = query($sql);
		$result = null;
		while ($row = mysql_fetch_object($res))
			$result[$row->type][$row->id] = $row->amount;
		return $result;
	}
}
?>