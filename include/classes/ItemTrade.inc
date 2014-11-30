<?php
require_once(dirname(__FILE__).'/Db.inc');
require_once(dirname(__FILE__).'/EventLog.inc');

/**
 * Diese Klasse ist für das Handeln von Items zuständig. Ein User kann Items
 * auf dem "Marktplatz" anbieten, User können dann für das Item Edelsteine
 * bieten.
 *
 * Unterstützt wird zudem das Verschenken und das Wegschmeißen von Gütern.
 *
 * Auch können hierüber Items von uns gekauft werden.
 *
 * @author Philipp Kemmeter
 */

class ItemTrade {
	const ERR_ITEM_DOES_NOT_EXIST = 1;
	const ERR_NOT_ENOUGH_KLUNKER = 2;
	const ERR_INFO_MSG_NOT_SENT = 3;

	const OFFER_TIMEOUT = 259200;	// 3 Tage in Sekunden (3*24*60*60)
	const LOCK_TIMEOUT = 900;		// 15min in Sekunden
	const LOCK_VISIT = 900;			// 15min in Sekunden

	/**
	 * Konstanten, welche Shops es gibt (Bronze, Silber, Gold)
	 */
	const SHOP_BRONZE = 1;
	const SHOP_SILVER = 2;
	const SHOP_GOLD = 3;

	private $uid;
	private static $INSTANCES = array();

	/**
	 * Gibt die Instanz für den User mit der angegebenen ID zurück
	 *
	 * @param uint $uid
	 * @return ItemTrade
	 */
	public static function get_instance($uid) {
		if (!isset($INSTANCES[$uid]))
			$INSTANCES[$uid] = new self($uid);
		return $INSTANCES[$uid];
	}

	/**
	 * Erzeugt eine neue Instanz für den User mti der angegebenen ID.
	 *
	 * @param uint $uid
	 * @return ItemTrade
	 */
	private function __construct($uid) {
		self::check_timouts();
		$this->uid = $uid;
	}

	/**
	 * Legt die Items aus dem Nichts in die Schatzkiste des Spielers mit
	 * der angegebenen ID. Hierüber können also Spieler vom System beschenkt
	 * werden.
	 *
	 * @param uint $uid
	 * @param uint[] $items
	 */
	public static function put_items_in_chest($uid, $items) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+",$uid);
		}
		$uid = (int)$uid;
		foreach ($items as $type => $id) {
			if ($type <= 0) {
				throw new IllegalArgumentException("type", "N+", $type);
			}
			if ($id <= 0) {
				throw new IllegalArgumentException("items[$type]", "N+", $id);
			}
		}

		$sql =
			"INSERT INTO user_items (uid, type, id, amount_mp, exclude_direct) ".
			"VALUES ";
		$first = true;
		foreach ($items as $type => $id) {
			if ($first)
				$first = false;
			else
				$sql .= ',';

			$sql .= "($uid, $type, $id, 1, 0) ";
		}
		$sql .= "ON DUPLICATE KEY UPDATE amount_mp=amount_mp+1, ".
			"exclude_direct=exclude_direct+1";
		query($sql);
	}

	/**
	 * Setzt die initialen Schatzkisten Items. Wenn der User bereits welche hat,
	 * werden keine Items hinzugefügt. Das dient zusätzlicher Sicherheit.
	 */
	public function init_items() {
		query("BEGIN");
		$res = query("SELECT COUNT(*) FROM user_items WHERE uid={$this->uid}");
		if (@mysql_result($res,0) > 0) {
			query("COMMIT");
			return;
		}
		$i = 0;
		while (($type = @constant("CONF_INITIAL_CLOTHES_{$i}_TYPE")) &&
			($id = @constant("CONF_INITIAL_CLOTHES_{$i}_ID")))
		{
			query(
				"INSERT INTO user_items SET id=$id, type=$type, amount_mp=1,".
					"uid={$this->uid}"
			);
			$i++;
		}
		query("COMMIT");
	}


	/**
	 * Prüft, ob Angebote noch vorhanden oder bereit abgelaufen sind.
	 */
	public static function check_timouts() {
		query("START TRANSACTION");
		$res = query(
			"SELECT * FROM item_trade_offers WHERE t_stamp<".
				(Time::get_t_now()-self::OFFER_TIMEOUT)
		);
		while ($row = mysql_fetch_object($res)) {
			query(
				"UPDATE klunker SET amount=amount+{$row->price} ".
					"WHERE uid={$row->buyer_id}"
			);
			//TODO Message
		}
		query (
			"DELETE FROM item_trade_offers WHERE t_stamp<".
				(Time::get_t_now()-self::OFFER_TIMEOUT)
		);
		query("COMMIT");
	}

	/**
	 * Gibt alle Items des angegebenen Typs zurück, die man kaufen kann.
	 *
	 * Wird zudem <code>$shop</code> angegeben, kann danach gefiltert werden,
	 * in welchem Geschäft sich das Item befinden soll (entspricht der
	 * Preisklasse).
	 *
	 * Mithilfe von <code>$uids</code> kann ein Array von User-IDs bestimmt
	 * werden, dass nur diejenigen Artikel zurückgegeben werden, die auch
	 * mindestens einer der angegebenen User hat.
	 *
	 *
	 * Die Rückgabe ist ein Array von stdClass:
	 *
	 *  result[i]->id 		ID des Items in Bezug auf $type
	 *  result[i]->cost 	Kosten des Items in Klunker (alle Rabatte inkl.)
	 *  result[i]->amount	Wie viele Items davon auf einmal gekauft werden
	 *  					müssen (Cost bezieht sich immer auf's Gesamtpaket)
	 *  result[i]->shop_id  In welchem Shop es das Item gibt
	 *  result[i]->d_discount Wie hoch der aktuelle, zeitlich beschränkte Rabatt
	 *  					  ist
	 *  result[i]->q_discount Wie hoch der aktuelle Mengenrabatt ist
	 *  result[i]->available Ob das Item aktuell verfügbar ist oder nicht
	 *  result[i]->av_until	Bis wann das Item verfügbar bleibt. 0 heißt für
	 *  					immer, wenn avaiable==true, sonst keine Angabe.
	 *  result[i]->uid		ID eines exemplarischen Users, der das Item besitzt
	 *
	 * @param uint $type
	 * @param uint $shop [default: false]
	 * @param uint $uids [default: false]
	 * @return stdClass[]
	 */
	public static function get_all_items($type, $shop=false, $uids=false) {
		$type = (int)$type;
		$shop = (int)$shop;

		$sql =
			"SELECT iu.uid, i1.type, i1.id, i1.days_in_shop, ". /*ALT: Paktezwang: */
				//"IF(i3.amount IS NULL, IF(i1.shop_id=2, 0, 1), i3.amount) ".
				"IF(i3.amount IS NULL, 1, i3.amount) "./*KEIN Packetzwang mehr*/
				"AS amount, i1.cost, i1.shop_id, ".
				"IF(i2.discount IS NULL, 0, i2.discount) AS d_discount, ".
				"i3.discount AS q_discount, ".
				"IF((i4.start_tstamp IS NOT NULL) ". /* start-t vergangen => 1*/
					"AND (i4.start_tstamp <= ".Time::get_t_now(). "), 1, 0) ".
					"AS available, ".
				"IF((i4.start_tstamp IS NULL) "./* start-t in Zukunft => 0*/
					"OR (i4.start_tstamp > ".Time::get_t_now(). ") ".
					"OR (i4.lasts_until_tstamp=4294967295), ".
													"0, i4.lasts_until_tstamp) ".
					"AS av_until ".
			"FROM items AS i1 ".
			"LEFT JOIN item_discount_duration AS i2 ".
				"ON i1.id=i2.id AND i1.type=i2.type ".
				"AND i2.start_tstamp<=".Time::get_t_now(). " ".
				"AND i2.lasts_until_tstamp>=".Time::get_t_now(). " ".
			"LEFT JOIN item_discount_quantity AS i3 ".
				"ON i1.type=i3.type AND i1.id=i3.id ".
			"LEFT JOIN item_available AS i4 ".
				"ON i4.type=i1.type AND i4.id=i1.id ".
				"AND i4.lasts_until_tstamp>".Time::get_t_now(). " ".
			"LEFT JOIN user_items AS iu ".
				"ON iu.type=i1.type AND iu.id=i1.id AND iu.amount_mp>0 ";
		if ($uids)
			$sql .= "AND iu.uid IN ('".implode("','", $uids)."') ";
		$sql .= "WHERE i1.type=$type";
		if ($shop > 0)
			$sql .= " AND i1.shop_id=$shop";
		if ($uids) {
			/**
			 * Wenn UID-Filter, dann diesen Berücksichtigen, aber das, was wir
			 * verkaufen und später als Available oben zurückgegeben wird, das
			 * wird immer akzeptiert.
			 */
			$sql .=
				" AND (iu.uid IS NOT NULL OR ((i4.start_tstamp IS NOT NULL) ".
					"AND (i4.start_tstamp <= ".Time::get_t_now(). ")))";
		}
		$sql .= " GROUP BY id, type";
		$sql .= " ORDER BY av_until, 2000000000-i4.start_tstamp, ".
					"id,type,amount";
		$res = query($sql);

		$result = array();
		while ($row = mysql_fetch_object($res)) {
			if ($row->amount <= 0)
				continue;

			$row->cost = floor(floor($row->cost * $row->amount
				* (100-$row->q_discount)/100)
				* (100-$row->d_discount)/100);
			$row->available = ($row->available == 0) ? false : true;

			$result[] = $row;
		}
		return $result;
	}

	/**
	 * Gibt für jede Schatzkiste, in dem das durch $type und $id bestimmte Item
	 * vorkommt, das entsprechende, dort direkt zum Verkauf stehende Item
	 * zurück.
	 * Wird $limit angegeben, kann das Ergebnis der Items auf eine bestimmt
	 * Anzahl von Items reduziert werden.
	 *
	 * Die Rückgabe ist bei Direktverkauf prinzipiell nach Schatzkistenbesuchern
	 * aufsteigend sortiert.
	 *
	 * result[i]->uid		Wem das Item gehört
	 * result[i]->price		Angegebener Mindestpreis
	 * result[i]->amount	Wieviele davon noch direkt gekauft werden können
	 * result[i]->visitors	Wieviele Nutzer die Schatzkiste gerade anschauen
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $limit
	 * @param uint[] $include_only_uids	Nur diese User werden zurückgegeben
	 * @param uint[] $exclude_uids		Diese User werden ausgeschlossen
	 * @return stdClass[]
	 */
	public static function get_all_chest_direct_instances_of_item($type, $id,
		$limit=0, $include_only_uids=array(), $exclude_uids=array())
	{
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		$type = (int)$type;
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}
		$id = (int)$id;

		query("START TRANSACTION");
		query("DELETE FROM user_items_locks WHERE t_stamp<".
			(Time::get_t_now()-self::LOCK_TIMEOUT));/** nach Xmin,Locks lösen */

		$sql =
			"SELECT COUNT(cv.visitor) AS visitors, ".
				"IF (ul.amount > 0, ". /** Locks included :) */
							"ui.amount_mp-ui.exclude_direct-ul.amount, ".
							"ui.amount_mp-ui.exclude_direct) AS amount, ".
				"ui.price, ui.uid ".
			"FROM user_items AS ui ".
			"LEFT JOIN user_chest_visits AS cv ON cv.uid=ui.uid ".
			"LEFT JOIN user_items_locks AS ul ".
				"ON ul.uid=ui.uid AND ul.type=ui.type AND ul.id=ui.id ".
			"WHERE ui.type=$type AND ui.id=$id AND ui.amount_mp>0 AND ".
				"ui.exclude_direct<ui.amount_mp AND ui.price>0 ";

		if (!empty($include_only_uids))
			$sql .=
				"AND ui.uid IN (".implode(",", $include_only_uids).") ";
		if (!empty($exclude_uids))
			$sql .=
				"AND ui.uid NOT IN (".implode(",", $exclude_uids).") ";

		$sql .=
			"GROUP BY uid ORDER BY cv.visitor ASC";

		if ($limit > 0)
			$sql .= " LIMIT $limit";
		$res = query($sql);
		query("COMMIT");
		$result = array();
		while ($row = mysql_fetch_object($res)) {
			if ($row->amount > 0)
				$result[] = $row;
		}

		return $result;
	}

	/**
	 * Gibt für jede Schatzkiste, in dem das durch $type und $id bestimmte Item
	 * vorkommt, das entsprechende, dort durch Handel/Bieten kaufbare Item
	 * zurück. Items, die gleichzeitig auch zum Direktkauf stehen, werden hier
	 * NICHT angezeigt.
	 * Wird $limit angegeben, kann das Ergebnis der Items auf eine bestimmt
	 * Anzahl von Items reduziert werden.
	 *
	 * Die Rückgabe ist bei Handel zufällig.
	 *
	 * result[i]->uid		Wem das Item gehört
	 * result[i]->amount	Wieviele davon noch zum Handel stehen
	 * result[i]->visitors	Wieviele Nutzer die Schatzkiste gerade anschauen
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $limit
	 * @param uint[] $include_only_uids	Nur diese User werden zurückgegeben
	 * @param uint[] $exclude_uids		Diese User werden ausgeschlossen
	 * @return stdClass[]
	 */
	public static function get_all_chest_trade_instances_of_item($type, $id,
		$limit=0, $include_only_uids=array(), $exclude_uids=array())
	{
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		$type = (int)$type;
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}
		$id = (int)$id;

		$sql =
			"SELECT COUNT(cv.visitor) AS visitors, ".
				"ui.amount_mp-ui.exclude_direct AS amount, ".
				"ui.price, ui.uid ".
			"FROM user_items AS ui ".
			"LEFT JOIN user_chest_visits AS cv ON cv.uid=ui.uid ".
			"WHERE ui.type=$type AND ui.id=$id AND ui.amount_mp>0 AND ".
				"ui.exclude_direct<ui.amount_mp AND ui.price=0 ";

		if (!empty($include_only_uids))
			$sql .=
				"AND ui.uid IN (".implode(",", $include_only_uids).") ";
		if (!empty($exclude_uids))
			$sql .=
				"AND ui.uid NOT IN (".implode(",", $exclude_uids).") ";

		$sql .=
			"GROUP BY uid";

		$res = query($sql);
		$result = array();
		while ($row = mysql_fetch_object($res))
			$result[] = $row;
		if (($limit > 0) && (count($result) > $limit)) {
			$tmp = $result;
			$result = array();
			$i = 0;
			while (count($result > 0) && ($i < $limit)) {
				$ri = rand(0,count($result)-1);
				$tmp[] = $result[$ri];
				array_splice($result, $ri, 1);
				$i++;
			}
			$result = $tmp;
		}

		return $result;
	}

	/**
	 * Gibt alle Items des angegebenen Typs zurück, die man aus anderen
	 * Schatzkisten heraus direkt kaufen kann (weil der Verkäufer einen Preis
	 * festgesetzt hat). Es wird pro Item immer nur ein Verkäufer angegeben und
	 * der Preis, den er verlangt, auch wenn es mehrere gibt. Die Auswahl des
	 * Verkäufers erfolgt anhand gewisser statischer Merkmale.
	 * Wird zudem $shop angegeben, kann danach gefiltert werden, in welchem
	 * Geschäft das Item ursprünglich vom Verkäufer gekauft wurde (entspricht
	 * der Preisklasse).
	 * $limit kann genutzt werden, um die Anzahl der Rückgabe-Werte zu
	 * begrenzen.
	 * Wird $lock_items_for_user auf TRUE gesetzt, werden die Items für "mich"
	 * bei Rückgabe reserviert, so dass diese Items kein anderer User angezeigt
	 * bekommt, bis "ich" "meinen" Lock aufgehoben habe oder ein Timeout
	 * erreicht wird.
	 *
	 * Die Rückgabe ist ein Array von stdClass-Objekten:
	 *
	 *  result[i]->id 		ID des Items in Bezug auf $type
	 *  result[i]->price 	Preis des Items in Klunker
	 *  result[i]->amount	Wie viele Items der User davon
	 *  result[i]->shop_id  In welchem Shop es das Item normalerweise gibt
	 *  result[i]->uid		Die ID des Verkäufers
	 *
	 * @param uint $type
	 * @param uint $shop [default: false]
	 * @param uint $limit [default: 0]
	 * @return stdClass[]
	 */
	public function get_all_direct_trade_items($type, $shop=false,
		$limit=0, $lock_items_for_user=false)
	{
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		$type = (int)$type;
		if ($shop < 0) {
			throw new IllegalArgumentException("shop", "N+\{0}",$shop);
		}
		$shop = (int)$shop;
		$limit = (int)$limit;

		query("START TRANSACTION");
		query("DELETE FROM user_items_locks WHERE t_stamp<".
			(Time::get_t_now()-self::LOCK_TIMEOUT));/** nach Xmin,Locks lösen */

		$sql =
			"SELECT ui.id, IF (ul.amount > 0, ". /** Locks included :) */
							"ui.amount_mp-ui.exclude_direct-ul.amount, ".
							"ui.amount_mp-ui.exclude_direct) AS amount, ".
				"ui.price, ui.uid, i.shop_id ".
			"FROM user_items AS ui ".
			"LEFT JOIN items AS i ".
				"ON i.id=ui.id AND i.type=ui.type ".
			"LEFT JOIN user_items_locks AS ul ".
				"ON ul.id=ui.id AND ui.type=ul.type AND ul.uid=ui.uid ".
			"WHERE ui.type=$type AND ui.amount_mp>0 AND ui.price>0 ".
				"AND ui.exclude_direct<ui.amount_mp";
		if ($shop > 0)
			$sql .= " AND i.shop_id=$shop";
		else
			$sql .= " AND i.shop_id IS NOT NULL";
		$sql .= " ORDER BY ui.id";

		$res = query($sql);

		$items = array();
		while ($row = mysql_fetch_object($res)) {
			if ($row->amount > 0)	// Kann durch Locks 0 werden
				$items[$row->id][] = $row;
		}


		$items_found = 0;
		$result = array();
		/**
		 * Pro ID soll es nur ein Item geben, also nur einen Verkäufer. Es wird
		 * jetzt in der nächsten Schleife anhand von verschiedenen Kriterien
		 * Verkäufer rausgeworfen und dann von den übrigen einer zufällig
		 * gewählt.
		 */
		foreach ($items as $id => $data) {
			/**
			 * Es werden nun zunächst alle Preis-Ausreißer extrahiert, also alle
			 * Angebote für Items mit besonders hohem oder besonders niedrigem
			 * Preis. Diese sollen niemals zurückgegeben werden.
			 */
			$n = count($data);
			if ($n == 1) {
				// Wenn nur einer da, dann den nehmen.
				$row_chosen = $data[0];
			}
			else {
				$m = 0;	// Mittelwert
				for ($i = 0; $i < $n; $i++) {
					$m += $data[$i]->price;
				}
				$m /= $n;

				$v = 0; // Varianz
				for ($i = 0; $i < $n; $i++) {
					$v += pow($data[$i]->price - $m, 2);
				}
				$v /= ($n - 1);

				$s = sqrt($v); // Standardabweichung

				$max = $m + 1.2*$s; // Maximalwert
				$min = $m - 1.2*$s; // Minimalwert


				for ($i = 0; $i < $n; $i++) {
					if (($data[$i]->price > $max)
						|| ($data[$i]->price < $min))
					{
						unset($data[$i]);
					}
				}

				/**
				 * Jetzt wird aus den übrigen einer zufällig gelost.
				 */
				$row_chosen = $data[rand(0, count($data)-1)];
			}
			if (!$row_chosen->uid)
				continue;
			$result[] = $row_chosen;
			if ($lock_items_for_user) {
				query(
					"INSERT INTO user_items_locks ".
					"SET locker_uid={$this->uid}, type=$type, ".
						"id={$row_chosen->id},uid={$row_chosen->uid},amount=1,".
						"t_stamp=".Time::get_t_now()." ".
					"ON DUPLICATE KEY UPDATE uid={$row_chosen->uid}, ".
						"t_stamp=".Time::get_t_now()
				);
			}
			$items_found++;
			/**
			 * Limitcheck: Schon genug Items hinzugefügt? Abbruch.
			 */
			if (($limit > 0) && ($items_found >= $limit))
				break;
		}
		query("COMMIT");
		return $result;
	}

	/**
	 * Löst alle Sperren, die "ich" ausgelöst habe.
	 */
	public function release_all_locks() {
		query("DELETE FROM user_items_locks WHERE locker_uid={$this->uid}");
	}

	/**
	 * Gibt alle Items von "mir" zurück.
	 *
	 * Es darf nach <code>$type</code> gefiltert werden.
	 *
	 * Die Rückgabe ist ein Array von stdClass:
	 *
	 *  result[i]->id 			ID des Items in Bezug auf $type
	 *  result[i]->amount 		Wie viele ich davon habe und nicht anbiete
	 *  result[i]->amount_mp	Wie viele ich davon zum Verkauf anbiete
	 *  result[i]->type     	Der Typ des Items
	 *  result[i]->shop_id		ID des Shops
	 *  result[i]->price		Der Preis, den ich mindestens dafür
	 *  						haben will. Bei 0 muss ein Preis ausgehandelt
	 *  						werden.
	 *  result[i]->days_in_shop	Wie lange der Schatz (angeblich) im Shop war
	 *  result[i]->exclude_direct	Wie viele davon ich nicht direkt verkaufe
	 *  result[i]->shop_price	Preis, den das Item im Shop kostet
	 *  result[i]->locked		Gibt an, wie viele der Items auf dem Marktplatz
	 *  						wegen Locks z.Z. nicht verfügbar sind.
	 *  result[i]->offers = array();
	 *  result[i]->offers[j]->buyer_id 	ID desjenigen, der Angebot gemacht
	 *  result[i]->offers[j]->price		Preis, den er geboten hat
	 *  result[i]->offers[j]->t_stamp	Zeitpunkt, wann er das Angebot machte
	 *
	 * Wenn $type niht angegeben wird, wird ein zweidimensonales Array
	 * zurückgegeben. 1. Dimension: $type, 2. Dimension wie eben beschrieben.
	 *
	 * @param uint $type  [optional]
	 * @return stdClass[]/stdClass[][]
	 */
	public function get_my_items($type=false, $amount_total_all_filter=null) {
		query("START TRANSACTION");
		query("DELETE FROM user_items_locks WHERE t_stamp<".
			(Time::get_t_now()-self::LOCK_TIMEOUT));/** nach Xmin,Locks lösen */
		$type = (int)$type;
		$sql =
			"SELECT ui.type,ui.id,ui.amount,ui.amount_mp,ui.price AS min_price, ".
				"ui.exclude_direct, ito.buyer_id,ito.price,ito.t_stamp, ".
				"i.shop_id, i.cost, i.days_in_shop, ul.amount AS locked ".
			"FROM user_items AS ui ".
			"LEFT JOIN items AS i ON i.type=ui.type AND i.id=ui.id ".
			"LEFT JOIN item_trade_offers AS ito ON ito.type=ui.type ".
				"AND ito.seller_id=ui.uid AND ito.id=ui.id ".
			"LEFT JOIN user_items_locks AS ul ON ul.type=ui.type ".
				"AND ul.uid=ui.uid AND ui.id=ul.id ".
			"WHERE ui.uid={$this->uid} AND (ui.amount>0 OR ui.amount_mp>0)";

		if ($type > 0)
			$sql .= " AND ui.type={$type}";

		$sql .= " ORDER BY type";

		$res = query($sql);
		query("COMMIT");

		$result = array();
		$added_items = array();
		while ($row = mysql_fetch_object($res)) {
			if (isset($added_items[$row->type][$row->id])) {
				$index = $added_items[$row->type][$row->id];
				$offer = new stdClass();
				$offer->buyer_id = $row->buyer_id;
				$offer->price = $row->price;
				$offer->t_stamp = $row->t_stamp;
				if ($type > 0) {
					$result[$index]->offers[] = $offer;
				}
				else {
					$result[$row->type][$index]->offers[] = $offer;
				}
			}
			else {
				$item = new stdClass();
				$item->type = $row->type;
				$item->id = $row->id;
				$item->amount = $row->amount;
				$item->amount_mp = $row->amount_mp;
				$item->shop_id = $row->shop_id;
				$item->exclude_direct = $row->exclude_direct;
				$item->shop_price = $row->cost;
				$item->locked = $row->locked;
				$item->days_in_shop = $row->days_in_shop;
				if ($row->exclude_direct < $row->amount_mp)
					$item->price = $row->min_price;
				else
					$item->price = 0;
				$item->offers = array();
				if (!empty($row->buyer_id) && !empty($row->price)) {
					$offer = new stdClass();
					$offer->buyer_id = $row->buyer_id;
					$offer->price = $row->price;
					$offer->t_stamp = $row->t_stamp;
					$item->offers[] = $offer;
				}
				if ($type > 0) {
					$added_items[$row->type][$row->id] = count($result);
					$result[] = $item;
				}
				else {
					$added_items[$row->type][$row->id] =
						isset($result[$row->type])
							? count($result[$row->type])
							: 0;
					$result[$row->type][] = $item;
				}
			}
		}
		return $result;
	}

	public function get_my_outgoing_offers() {
		$res = query(
			"SELECT seller_id, type, id, price, t_stamp ".
			"FROM item_trade_offers WHERE buyer_id={$this->uid}"
		);
		$result = array();
		while ($row = mysql_fetch_object($res)) {
			$result[] = $row;
		}
		return $result;
	}

	/**
	 * Gibt zurück, wie viele Items "ich" zur Zeit insgesamt besitze.
	 *
	 * @return uint
	 */
	public function get_my_items_amount() {
		return @mysql_result(query(
			"SELECT SUM(amount_mp)+SUM(amount) FROM user_items ".
			"WHERE uid={$this->uid}"
		),0);
	}

	/**
	 * Gibt zurück, wie viele Angebote der $this-User gerade insgesamt hat.
	 * Der Parameter $grouped bestimmt hierbei, ob die Summe pro Item oder Typ
	 * als mehrdimensionales Array zurückgegeben werden soll, oder, wenn
	 * false, die Gesamtsumme gewünscht ist. Das Mehrdimensionale Array hätte
	 * die Struktur:
	 * 		array[$type][$id] = $amount
	 *
	 * Mithilfe von $type und $id kann die Anfrage näher bestimmt werden (z.B.
	 * wie viele Angebote hat der User bei Kopfbedeckungen? Wie viele bei
	 * Helmen?).
	 *
	 * @param bool $grouped [default: true]
	 * @param uint $type	[default: false]
	 * @param uint $id		[default: false]
	 * @return uint/uint[][]
	 */
	public function get_my_offers_amounts($grouped=true, $type=false, $id=false)
	{
		if ($type !== false) {
			$type = (int)$type;
			if ($type <= 0) {
				throw new IllegalArgumentException("type", "N+\{0}",$type);
			}
		}
		if ($id !== false) {
			$id = (int) $id;
			if ($id <= 0) {
				throw new IllegalArgumentException("id", "N+\{0}",$id);
			}
		}
//		self::check_timouts();

		if ($grouped) {
			$sql =
				"SELECT type, id, COUNT(*) as amount FROM item_trade_offers ".
				"WHERE seller_id={$this->uid}";
			if ($type)
				$sql .= " AND type=$type";
			if ($id)
				$sql .= " AND id=$id";

			$sql .= " GROUP BY type, id";
			$res = query($sql);

			$result = null;
			while ($row = mysql_fetch_assoc($res)) {
				$result[$row['type']][$row['id']] = $row['amount'];
			}
			return $result;
		}
		else {
			$sql =
				"SELECT COUNT(*) FROM item_trade_offers ".
				"WHERE seller_id={$this->uid}";
			if ($type) {
				 $sql .= " AND type=$type";
			}
			if ($id)
				$sql .= " AND id=$id";
			$res = query($sql);
			return (int)(@mysql_result($res, 0));
		}

	}

	/**
	 * Hilfsfunktion die das Kaufen erledigt. Wird $reduce_klunkers_only auf
	 * true gesetzt, dann wird der Kauf nicht durchgeführt, die Klunker dafür
	 * aber abgezogen. Das macht Sinn, wenn die Gegenleistung schon erbracht
	 * wurde (s. use_items)
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $amount
	 * @param bool $reduce_klunkers_only
	 * @return uint
	 */
	private function buy_item_helper($type, $id, $amount,
		$reduce_klunkers_only=false)
	{
		/**
		 * Test: Ist das Item aktuell überhaupt verfügbar, oder ist es
		 * ausverkauft? Da es keine Mengen im Shop gibt, sondern nur Zeiträume,
		 * in denen es bestimmte Items nicht mehr gibt, wird hier anders
		 * geprüft, als man intuitiv denken würde.
		 */
		$res = query(
			"SELECT lasts_until_tstamp FROM item_available ".
			"WHERE id=$id AND type=$type ".
				"AND start_tstamp>=".(Time::get_t_now())." ".
				"AND lasts_until_tstamp<".(Time::get_t_now() + 30)
		);
		if (mysql_num_rows($res) > 0) {
			throw new Exception("Item does not exist!",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		$res = query(
			"SELECT amount FROM klunker WHERE uid={$this->uid}"
		);
		$kkr_amount = (int)(@mysql_result($res,0));
		$res = query(
			"SELECT i1.cost, i1.shop_id, ".
				"IF(i2.discount IS NULL, 0, i2.discount) AS d_discount, ".
				"i3.discount AS q_discount ".
			"FROM items AS i1 ".
			"LEFT JOIN item_discount_duration AS i2 ".
				"ON i1.id=i2.id AND i1.type=i2.type ".
				"AND i2.lasts_until_tstamp>=".(Time::get_t_now() - 30)." ".
				"AND i2.start_tstamp<".(Time::get_t_now()). " ".
			"LEFT JOIN item_discount_quantity AS i3 ".
				"ON i1.type=i3.type AND i1.id=i3.id AND i3.amount=$amount ".
			"WHERE i1.type=$type AND i1.id=$id"
		);
		if (mysql_num_rows($res) == 0) {
			throw new Exception("Item does not exist!",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		$item_row = mysql_fetch_assoc($res);
		/**
		 * Test: Gibt es dieses Item überhaupt in der gewünschten Mengenabgabe?
		 */
		// Im Silbershop sind grundsätzlich keine Einzeleinkäufe möglich
		/* DIESES FEATURE IST DEAKTIVIERT!
		if (($amount == 1) && ($item_row['shop_id'] == self::SHOP_SILVER)) {
			throw new Exception("Item does not exist!",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}*/
		// Es ist ein Paket ($amount>1) gewünscht, aber es gibt kein solches
		// Paket für das angegebene Item (sonst gäbe es einen q_discount)
		if (($amount > 1) && ($item_row['q_discount'] === null)) {
			throw new Exception("Item does not exist!",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		/**
		 * Zunächst den Grundpreis ermitteln: $item_row['cost']*amount.
		 * Dann den Mengenrabatt draufrechnen und das Ergebnis runden. Das ist
		 * der Preis, den der User zahlt, wenn es keinen zeitlichen Rabatt gibt.
		 * Nun nach der Rundung den Zeitlichen Rabatt einrechnen. Somit ist das
		 * Ergebnis das, was der Nutzer erwartet, nämlich X% vom sowieso
		 * reduzierten Preis. Ein leichter Unterschied von vielleich 1-2Klunker
		 * würde es machen, wenn man erst allem Multipliziert und am Ende
		 * rundet.
		 */
		$kkr_needed = floor(floor($item_row['cost'] * $amount
			* (100-$item_row['q_discount'])/100)
			* (100-$item_row['d_discount'])/100);

			if ($kkr_amount < $kkr_needed) {
			throw new Exception("Not enough klunker! ".
				"|$kkr_needed,$kkr_amount",
				self::ERR_NOT_ENOUGH_KLUNKER);
		}
		query (
			"UPDATE klunker SET amount=amount-$kkr_needed WHERE uid={$this->uid}"
		);
		if (!$reduce_klunkers_only) {
			query (
				"INSERT INTO user_items SET id=$id, type=$type, uid={$this->uid}, ".
					"amount_mp=$amount ".
				"ON DUPLICATE KEY UPDATE amount_mp=amount_mp+$amount"
			);
		}
		return $kkr_needed;
	}

	/**
	 * Kauft ein Item von uns. D.h. es werden die Klunker abgebucht, die er
	 * benötigt, um das Item zu kaufen, und das Item zu seinen Items
	 * hinzugefügt. $amount gibt dabei an, wie viele Items der Art auf einmal
	 * gekauft werden sollen. Dabei werden Mengenrabatte (Paketpreise)
	 * berücksichtigt.
	 * Gibt zurück, wie viel das Item gekostet hat bzw. die Items gekostet
	 * haben.
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $amount	[default: 1]
	 * @return uint
	 * @throws IllegalArgumentException
	 * @throws Exception
	 */
	public function buy_item($type, $id, $amount=1) {
		$id = (int)$id;
		$type = (int)$type;
		$amount = (int)$amount;
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}
		if ($amount <= 0) {
			throw new IllegalArgumentException("amount", "N+\{0}", $amount);
		}
		query("START TRANSACTION");
		try {
			$kkr_needed = $this->buy_item_helper($type, $id, $amount);
		}
		catch (Exception $e) {
			query("ROLLBACK");
			throw $e;
		}
		query("COMMIT");

		/**
		 * Test-Aktionen nicht loggen
		 */
		$log_db = new Db(Db::LOGS_HOST,Db::LOGS_NAME,
								Db::LOGS_PW, Db::LOGS_SELECT_DB);
		$log_db->query(
			"INSERT INTO item_buy_actions SET id=$id, type=$type, ".
				"uid={$this->uid}, price=$kkr_needed, amount=$amount, ".
				"timestamp=".Time::get_t_now()
		);

		return $kkr_needed;
	}

	/**
	 * Diese Funktion verbraucht die angegebenen Items (z.B. Kleider).
	 *
	 * Sie werden direkt verbraucht, wenn sie in der eigenen Schatzkiste sind.
	 * Sind sie nicht vorhanden, aber $buy_if_na ist gesetzt (default), dann
	 * werden die fehlenden Items direkt von uns gekauft. Können nicht alle
	 * Items verbraucht werden, wird ein Fehler ausgeworfen und KEIN EINZIGES
	 * ITEM wird verbraucht.
	 *
	 * @param array $items_array
	 * @param uint $buy_if_na=true
	 */
	public function use_items(array $items_array, $buy_if_na=true)
	{
		$items_array;
		query("START TRANSACTION");
		foreach ($items_array as $type => $id) {
			if (!$id)	// Keine Kleidung des Typs gewählt
				continue;
			/**
			 * 1. Check, ob das gewünschte Kleidungsstück in Schatzkiste
			 */
			$res = query(
				"SELECT amount, amount_mp FROM user_items ".
				"WHERE uid={$this->uid} AND type=$type AND id=$id"
			);
			if (mysql_num_rows($res) > 0) {
				$row = mysql_fetch_assoc($res);
				$cur_amount = $row['amount'];
				$cur_amount_mp = $row['amount_mp'];
			}
			else {
				$cur_amount = $cur_amount_mp = 0;
			}

			if ($cur_amount > 0) {
				/**
				 * Fall 1: Es ist in der Schatzkiste => verbrauchen
				 */
				query(
					"UPDATE user_items SET amount=amount-1 ".
					"WHERE uid={$this->uid} AND type=$type AND id=$id"
				);
			}
			else if ($cur_amount_mp > 0) {
				/**
				 * Fall 2: Es ist auf dem Marktplatz => vom Marktplatz nehmen
				 * 	und verbrauchen.
				 */
				query(
					"UPDATE user_items SET amount_mp=amount_mp-1 ".
					"WHERE uid={$this->uid} AND type=$type AND id=$id"
				);
				/**
				 * TODO: Offers? Angebote sollte dann abegelehnt werden, wenn
				 * sie noch daraus waren usw... oder einfach auslaufen lassen?
				 */
			}
			else {
				/**
				 * Fall 3: Es ist nicht in der Schatzkiste => nachkaufen
				 */
				if (!$buy_if_na) {
					query("ROLLBACK");
					throw new Exception("Item ($type,$id) nicht in Schatztrue!",
						self::ERR_ITEM_DOES_NOT_EXIST);
				}
				// Es darf auch gekauft werden, also versuchen wir's:
//				// (ACHTUNG: copy-paste von buy_klunker)
//				$res = query(
//					"SELECT amount FROM klunker WHERE uid={$this->uid}"
//				);
//				$kkr_amount = (int)(@mysql_result($res,0));
//				$res = query(
//					"SELECT cost FROM items WHERE type=$type AND id=$id"
//				);
//				$kkr_needed = (int)(@mysql_result($res,0));
//				if (!$kkr_needed) {
//					query("ROLLBACK");
//					throw new Exception("Item does not exist!",
//						self::ERR_ITEM_DOES_NOT_EXIST);
//				}
//				if ($kkr_amount < $kkr_needed) {
//					query("ROLLBACK");
//					throw new Exception("Not enough klunker!",
//						self::ERR_NOT_ENOUGH_KLUNKER);
//				}
//				// Es hat geklappt: Klunker abziehen
//				$kkr_amount -= $kkr_needed;
//				query (
//					"UPDATE klunker SET amount=amount-$kkr_needed ".
//					"WHERE uid={$this->uid}"
//				);
				/*try {
					$kkr_needed = $this->buy_item_helper($type, $id, 1, true);
				}
				catch (Exception $e) {
					query("ROLLBACK");
					throw $e;
				}*/

				/**
				 * Aus der Schatzkiste der anderen nachkaufen! Das geht nur,
				 * wenn das Item vorher gelockt wurde. Durch die DB-Struktur
				 * ist sichergestellt, dass jeder User (locker_uid) nur ein
				 * Item einer Art locken kann (also nur einen grauen Helm).
				 * Somit ist klar, von wem er das Item nun kauft, um es zu
				 * verbrauchen.
				 *
				 * Hier wird nicht auf Timeouts von Locks geachtet. Der Vorteil
				 * ist, dass das Item dem User noch zugeordnet werden kann,
				 * auch wenn es nicht mehr von ihm reserviert ist (theoretisch).
				 * Praktisch kann es sein, dass ein Kleidungsstück nicht
				 * gefunden wird und somit nicht verbraucht werden kann, was
				 * zum Komplettabbruch führt (s.u.)
				 */
				$res = query(
					"SELECT ui.uid, ui.price FROM user_items_locks AS ul ".
					"LEFT JOIN user_items AS ui ON ui.uid=ul.uid ".
						"AND ui.type=ul.type AND ui.id=ul.id ".
					"WHERE ul.locker_uid={$this->uid} AND ul.type=$type ".
						"AND ul.id=$id AND ui.uid IS NOT NULL AND ui.price>0 ".
						"AND ui.amount_mp-ui.exclude_direct>0"
				);
				if (mysql_num_rows($res) == 0) {
					query("ROLLBACK");	// Komplettabbruch
					error_log("SELECT amount, amount_mp FROM user_items ".
				"WHERE uid={$this->uid} AND type=$type AND id=$id");
					throw new Exception("Item does not exist (user_items_locks)",
						self::ERR_ITEM_DOES_NOT_EXIST);
				}
				$row = mysql_fetch_object($res);
				$price = $row->price;
				$seller_id = $row->uid;

				/**
				 * 1. Klunkercheck: Hat er genug Klunker, um das Item zu kaufen?
				 */
				$res = query(
					"SELECT amount FROM klunker WHERE uid={$this->uid}"
				);
				$klunker_amount = @mysql_result($res,0);
				$new_amount = $klunker_amount - $price;
				if ($new_amount < 0) {
					query("ROLLBACK");
					throw new Exception("Not enough Klunker!",
						self::ERR_NOT_ENOUGH_KLUNKER);
				}

				/**
				 * 2. Direktverkauf findet statt, da genügend Objekte zum
				 * direkten Verkauf existieren und der vom Verkäufer angegebene
				 * Preis realistisch (d.h. >0) ist.
				 */
				/**
				 * Das item vernichten.
				 */
				query (
					"UPDATE user_items SET amount_mp=amount_mp-1 ".
					"WHERE uid=$seller_id AND type=$type AND id=$id"
				);

				/**
				 * Das Geld transferieren
				 */
				query(
					"UPDATE klunker SET amount=amount-$price ".
					"WHERE uid={$this->uid}"
				);
				query(
					"INSERT INTO klunker SET amount=$price, uid=$seller_id ".
					"ON DUPLICATE KEY UPDATE amount=amount+$price"
				);
			}
		}
		query("COMMIT");
	}


	/**
	 * Stellt das die durch Typ und ID identifizierte Item auf den virtuellen
	 * Marktplatz (so viele davon, wie $amount angibt).
	 *
	 * Gibt ein Array zurück:
	 * $result[0] = ANZAHL_IN_KISTE_NICHT_AUF_MP
	 * $result[1] = ANZAHL_AUF_MP
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $amount=1	// Wenn 0, dann alle
	 * @return uint[2]
	 */
	public function put_items_on_mp($type, $id, $amount=1) {
		$type = (int)$type;
		$id = (int)$id;
		$amount = (int)$amount;
		if ($amount < 0) {
			throw new IllegalArgumentException("amount", "N+",$amount);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}

		query("START TRANSACTION");
		$res = query(
			"SELECT amount,amount_mp FROM user_items ".
				"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		if (mysql_num_rows($res) > 0) {
			$cur_row = mysql_fetch_array($res);
			$cur_amount = $cur_row['amount'];
			$cur_amount_mp = $cur_row['amount_mp'];
		}
		else {
			query("ROLLBACK");
			throw new Exception("No such item owned.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		if ($cur_amount == 0)
			return array($cur_amount,$cur_amount_mp);

		if (!$amount || ($amount > $cur_amount))
			$amount = $cur_amount;


		query (
			"UPDATE user_items SET amount=amount-$amount, ".
				"amount_mp=amount_mp+$amount ".
			"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		query("COMMIT");
		return array($cur_amount-$amount,$cur_amount_mp+$amount);
	}


	/**
	 * Entfernt Items (Identifiziert durch Typ und ID) wieder vom Marktplatz
	 * (so viele, wie $amount angibt, wenn möglich, sonst soviele wie möglich).
	 * Gibt zurück, wie viele Items zurückgezogen wurden.
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $amount=1	// 0 == alle
	 * @return uint
	 */
	public function remove_item_from_mp($type, $id, $amount=1) {
		$type=(int)$type;
		$id=(int)$id;
		$amount = (int)$amount;
		if (($amount < 0) || ($type <= 0) || ($id <= 0))
			return 0;

		query("START TRANSACTION");
			$res = query(
			"SELECT amount,amount_mp FROM user_items ".
				"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		if (mysql_num_rows($res) > 0) {
			$cur_row = mysql_fetch_array($res);
			$cur_amount = $cur_row['amount'];
			$cur_amount_mp = $cur_row['amount_mp'];
		}
		else {
			query("ROLLBACK");
			throw new Exception("No such item owned.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		if ($cur_amount_mp == 0)
			return array($cur_amount,$cur_amount_mp);

		if (!$amount || ($amount > $cur_amount_mp))
			$amount = $cur_amount_mp;

		query (
			"UPDATE user_items SET amount=amount+$amount, ".
				"amount_mp=amount_mp-$amount ".
			"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		query("COMMIT");

		return array($cur_amount+$amount,$cur_amount_mp-$amount);
	}

	/**
	 * Lässt den User für eines seiner Items einen Mindestpreis festlegen.
	 * Gebote, die gleich oder darüber liegen, werden automatisch angenommen,
	 * was Direktkauf ermöglicht (Nutzer kennen den Mindestpreis vorher und
	 * können sich entscheiden es direkt zu kaufen).
	 *
	 * Mit dem Parameter $exclude kann festgelegt werden, wie viele Items er
	 * maximal über diese Option verkaufen möchte. Default ist 0.
	 *
	 * @param uint $type
	 * @param uint $id
	 * @param uint $price
	 * @param uint $exclude=0
	 */
	public function set_minimum_price($type, $id, $price, $exclude=0) {
		$type = (int)$type;
		$id = (int)$id;
		$price = (int)$price;
		$exclude = (int)$exclude;
		if ($exclude < 0) {
			throw new IllegalArgumentException("exclude", "N+",$exclude);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($price < 0) {
			throw new IllegalArgumentException("price", "N+",$price);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}

		query("START TRANSACTION");
		$res = query(
			"SELECT amount,amount_mp FROM user_items ".
				"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		if (mysql_num_rows($res) > 0) {
			$cur_row = mysql_fetch_object($res);
			$cur_amount = $cur_row->amount;
			$cur_amount_direct = $cur_row->amount_direct;
			$cur_amount_total = $cur_amount_direct + $cur_amount;
		}
		else {
			query("ROLLBACK");
			throw new Exception("No such item owned.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		if ($cur_amount_total > $exclude)
			$exclude = $cur_amount_total;

		query (
			"UPDATE user_items SET exclude_direct=$exclude, price=$price ".
			"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		query("COMMIT");
	}

	/**
	 * Es wird versucht dem User ein Item direkt abzukaufen. Das funktioniert
	 * nur dann, wenn der Verkäufer mindestens ein solches Item zum
	 * Direktverkauf freigegeben hat und der Käufe (logo) genug Geld hat, um
	 * es zu kaufen.
	 * Da die Klunker unmittelbar abgezogen werden, wird zurückgegeben, wie
	 * viele Klunker der User nach der Aktion noch hat.
	 *
	 * @param uint $seller_id
	 * @param uint $price		Preis, den der Käufer erwartet
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 */
	public function buy_directly_from_user($seller_id, $price, $type, $id) {
		//TODO $price!
		$seller_id = (int)$seller_id;
		$type = (int)$type;
		$id = (int)$id;
		if ($seller_id <= 0) {
			throw new IllegalArgumentException("seller_id", "N+\{0}",$seller_id);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}
		query("START TRANSACTION");
		/**
		 * 0. Zunächst wird der Mindestpreis des Items ermittelt, den der Nutzer
		 * angegeben hat. Wenn der Mindestpreis erfüllt ist, geschieht die
		 * Transaktion direkt und alles ist erledigt, ansonsten wird das Angebot
		 * hinterlegt und der Nutzer kann Verkäufe rentscheiden, ob er es
		 * annimmt oder ablehnt.
		 */
		$res = query(
			"SELECT price, amount_mp, amount, exclude_direct FROM user_items ".
			"WHERE uid=$seller_id AND id=$id AND type=$type"
		);
		if (mysql_num_rows($res) == 0) {
			query("ROLLBACK");
			throw new Exception("No such in treasurechest.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		$seller_user_items = mysql_fetch_object($res);

		$total_amount = $seller_user_items->amount
			+ $seller_user_items->amount_mp;

		if (($seller_user_items->amount_mp <= $seller_user_items->exclude_direct)
			|| ($seller_user_items->price <= 0))
		{
			query("ROLLBACK");
			throw new Exception("No such item on direct buy.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}

		$price = $seller_user_items->price;

		/**
		 * 1. Klunkercheck: Hat er genug Klunker, um das Item zu kaufen?
		 */
		$res = query(
			"SELECT amount FROM klunker WHERE uid={$this->uid}"
		);
		$klunker_amount = @mysql_result($res,0);
		$new_amount = $klunker_amount - $price;
		if ($new_amount < 0) {
			query("ROLLBACK");
			throw new Exception("Not enough Klunker!",
				self::ERR_NOT_ENOUGH_KLUNKER);
		}

		/**
		 * 2. Direktverkauf findet statt, da genügend Objekte zum direkten
		 * Verkauf existieren und der vom Verkäufer angegebene Preis
		 * realistisch ist.
		 */
		/**
		 * Das item verschieben
		 */
		query (
			"UPDATE user_items SET amount_mp=amount_mp-1 ".
			"WHERE uid=$seller_id AND type=$type AND id=$id"
		);

		$seller_user_items->amount_mp--;
		query (
			"INSERT INTO user_items SET uid={$this->uid}, type=$type, ".
				"id=$id, amount_mp=1 ".
			"ON DUPLICATE KEY UPDATE amount_mp=amount_mp+1"
		);

		/**
		 * Das Geld transferieren
		 */
		query(
			"UPDATE klunker SET amount=amount-$price WHERE uid={$this->uid}"
		);
		query(
			"INSERT INTO klunker SET amount=$price, uid=$seller_id ".
			"ON DUPLICATE KEY UPDATE amount=amount+$price"
		);
		query("COMMIT");

		// Ereignis-Log
		EventLog::get_instance($seller_id)->add_entry(
			EventLog::TYPE_ITEM_SOLD_DIRECTLY,
			$this->uid,
			"$type-$id-$price"
		);
		return $new_amount;
	}

	/**
	 * Macht dem User mit der UID $seller_id das Angebot, ein Item identifiziert
	 * durch $id und $type für $price Klunker abzukaufen.
	 * Da die Klunker unmittelbar abgezogen werden, wird zurückgegeben, wie
	 * viele Klunker der User nach der Aktion noch hat.
	 *
	 * @param uint $seller_id
	 * @param uint $price
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 * @throws IllegalArgumentException, Exception
	 */
	public function make_offer($seller_id, $price, $type, $id) {
		$seller_id = (int)$seller_id;
		$price = (int)$price;
		$type = (int)$type;
		$id = (int)$id;
		if ($seller_id <= 0) {
			throw new IllegalArgumentException("seller_id", "N+\{0}",$seller_id);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}
		if ($price <= 0) {
			throw new IllegalArgumentException("price", "N+\{0}",$price);
		}
		query("START TRANSACTION");
		/**
		 * 0. Klunkercheck: Kann er überhaupt das Angebot machen? Hat er genug
		 * Klunker?
		 */
		$res = query(
			"SELECT amount FROM klunker WHERE uid={$this->uid}"
		);
		$klunker_amount = @mysql_result($res,0);
		$new_amount = $klunker_amount - $price;
		if ($new_amount < 0) {
			query("ROLLBACK");
			throw new Exception("Not enough Klunker!",
				self::ERR_NOT_ENOUGH_KLUNKER);
		}

		/**
		 * 1. Es wird festgestellt, ob bereits ein Gebot für dasselbe Item
		 * vorliegt. Das muss dann zuerst zurückgezogen werden.
		 */
		$res = query(
			"SELECT price FROM item_trade_offers WHERE id=$id AND type=$type ".
				"AND seller_id=$seller_id AND buyer_id={$this->uid}"
		);
		$old_price = @mysql_result($res,0);
		if ($old_price > 0) {
			query(
				"DELETE FROM item_trade_offers WHERE id=$id AND type=$type AND ".
					"seller_id=$seller_id AND buyer_id={$this->uid}"
			);

			query(
				"UPDATE klunker SET amount=amount+$old_price ".
				"WHERE uid={$this->uid}"
			);
		}

		/**
		 * 2. Es wird versucht, das neue Angebot zu machen
		 */
		$res = query(
			"SELECT amount_mp, price, exclude_direct, amount FROM user_items ".
			"WHERE uid=$seller_id AND id=$id AND type=$type"
		);
		if (mysql_num_rows($res) == 0) {
			query("ROLLBACK");
			throw new Exception("No such item at all.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		$seller_user_items = mysql_fetch_object($res);

		if ($seller_user_items->amount_mp <= 0) {
			query("ROLLBACK");
			throw new Exception("No such item on MP atm.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		query(
			"INSERT INTO item_trade_offers SET id=$id, type=$type, ".
				"seller_id=$seller_id, buyer_id={$this->uid}, ".
				"price=$price, t_stamp=".Time::get_t_now()." ".
			"ON DUPLICATE KEY UPDATE price=$price, t_stamp=".Time::get_t_now()
		);
		query(
			"UPDATE klunker SET amount=amount-$price WHERE uid={$this->uid}"
		);
		query("COMMIT");
		/**
		 * Nun wird geschaut, ob das Angebot vielleicht schon höher ist, als
		 * der geforderte Mindestpreis. Im dem Fall kann es gleich angenommen
		 * werden.
		 */
		if ($seller_user_items->price && ($price >= $seller_user_items->price)
			&& ($seller_user_items->amount_mp >
				$seller_user_items->exclude_direct))
		{
			ItemTrade::get_instance($seller_id)->accept_offer(
				$this->uid, $type, $id
			);
		}
		return $new_amount+$old_price;
	}

	/**
	 * Zieht ein Angebot zurück, welches an den User mit der ID $seller_id
	 * gemacht wurde. Das zurückzuziehende Item wird identifiziert
	 * durch $id und $type.
	 *
	 * Gibt den Preis zurück, der angeboten wurde.
	 *
	 * @param uint $seller_id
	 * @param uint $type
	 * @param uint $id
	 * @throws IllegalArgumentException, Exception
	 */
	public function withdraw_offer($seller_id, $type, $id) {
		$seller_id = (int)$seller_id;
		$type = (int)$type;
		$id = (int)$id;

		if ($seller_id <= 0) {
			throw new IllegalArgumentException("seller_id", "N+\{0}",$seller_id);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}

		return self::remove_offer($this->uid, $seller_id, $type, $id);
	}

	/**
	 * Hilfsfunktion zum Entfernen von Angeboten, da withdraw_offer und
	 * reject_offer im Endeffekt dasselbe tun.
	 *
	 * Gibt den Preis zurück, der angeboten wurde.
	 *
	 * @param uint $buyer_id
	 * @param uint $seller_id
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 */
	private static function remove_offer($buyer_id, $seller_id, $type, $id) {
		query("START TRANSACTION");
		$res = query(
			"SELECT price FROM item_trade_offers WHERE id=$id AND type=$type ".
				"AND seller_id=$seller_id AND buyer_id=$buyer_id"
		);
		$price = @mysql_result($res,0);
		if (!$price) {
			query("ROLLBACK");
			throw new Exception("Offer does not exist (Maybe accepted?)",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		query(
			"DELETE FROM item_trade_offers WHERE id=$id AND type=$type AND ".
				"seller_id=$seller_id AND buyer_id=$buyer_id"
		);

		query(
			"UPDATE klunker SET amount=amount+$price WHERE uid=$buyer_id"
		);
		query("COMMIT");

		return $price;
	}

	/**
	 * Lehnt ein von $buyer_id gemachtes Angebot ab, welches durch die Daten
	 * genau spezifiziert ist.
	 *
	 * Gibt den Preis zurück, der angeboten wurde
	 *
	 * @param uint $buyer_id
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 * @throws IllegalArgumentException, Exception
	 */
	public function reject_offer($buyer_id, $type, $id) {
		$buyer_id = (int)$buyer_id;
		$type = (int)$type;
		$id = (int)$id;

		if ($buyer_id <= 0) {
			throw new IllegalArgumentException("buyer_id", "N+\{0}",$buyer_id);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}

		$price = self::remove_offer($buyer_id, $this->uid, $type, $id);

		// Ereignis-Log
		EventLog::get_instance($buyer_id)->add_entry(
			EventLog::TYPE_TRADE_DENIED,
			$this->uid,
			"$type-$id-$price"
		);

		return $price;
	}

	/**
	 * Akzeptiert ein "mir" gemachtes Angebot identifieziert durch die
	 * angegeben Daten.
	 *
	 * Gibt eine Struktur folgender Form zurück:
	 * $result->price		Wie viele Klunker der Verkäufer bekam
	 * $result->users_to_inform => stdClass[] mit folgendem:
	 * $result->users_to_inform[i]->uid		ID des Users, der eine Mail bekommen
	 * 										sollte, da sein Angebot automatisch
	 * 										zurückgewiesen wurde
	 * $result->users_to_inform[i]->price	Anzahl der Klunker, die der User
	 * 										zurückgebucht bekam
	 *
	 * @param uint $buyer_id
	 * @param uint $type
	 * @param uint $id
	 * @return stdClass
	 * @throws IllegalArgumentException, Exception
	 */
	public function accept_offer($buyer_id, $type, $id) {
		$buyer_id = (int)$buyer_id;
		$type = (int)$type;
		$id = (int)$id;

		if ($buyer_id <= 0) {
			throw new IllegalArgumentException("buyer_id", "N+\{0}",$buyer_id);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}
//		self::check_timouts();

		query("START TRANSACTION");
		$res = query(
			"SELECT price FROM item_trade_offers WHERE id=$id AND type=$type ".
				"AND seller_id={$this->uid} AND buyer_id=$buyer_id"
		);
		if (mysql_num_rows($res) == 0) {
			query("ROLLBACK");
			throw new Exception("Kein Angebot der Art gemacht!",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		$price = mysql_result($res,0);

		$res = query (
			"SELECT amount_mp FROM user_items WHERE uid={$this->uid} AND ".
				"type=$type AND id=$id"
		);
		$amount_mp = @mysql_result($res,0);
		if (!$amount_mp) {
			query("ROLLBACK");
			throw new Exception("Item not on MP. Maybe already traded?",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		/**
		 * Das item verschieben
		 */
		query (
			"UPDATE user_items SET amount_mp=amount_mp-1 ".
			"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		$amount_mp -= 1;

		query (
			"INSERT INTO user_items SET uid={$buyer_id}, type=$type, id=$id, ".
				"amount_mp=1 ".
			"ON DUPLICATE KEY UPDATE amount_mp=amount_mp+1"
		);

		/**
		 * Das Geld gutschreiben
		 */
		query(
			"UPDATE klunker SET amount=amount+$price WHERE uid={$this->uid}"
		);

		/**
		 * Angebot entfernen, da abgehandelt.
		 */
		query(
			"DELETE FROM item_trade_offers WHERE id=$id AND type=$type AND ".
				"seller_id={$this->uid} AND buyer_id=$buyer_id"
		);

		/**
		 * Alle Angebote für dieses Item zurückziehen, wenn keine Items dieser
		 * Art mehr im Schaufenster liegen, da hiermit das letzte verkauft
		 * wurde.
		 */
		if ($amount_mp <= 0) {
			$result_users_to_inform = array();
			$res = query(
				"SELECT price,buyer_id FROM item_trade_offers WHERE id=$id ".
					"AND type=$type AND seller_id={$this->uid}"
			);
			while ($row = mysql_fetch_object($res)) {
				/**
				 * Der User sollte per I/M oder Mail informiert werden. Dieses
				 * Array wird u.a. zurückgegeben.
				 */
				$info = new stdClass();
				$info->uid = $row->buyer_id;
				$info->price = $row->price;
				$result_users_to_inform[] = $info;

				/**
				 * Angebot zurückziehen
				 */
				query(
					"DELETE FROM item_trade_offers WHERE id=$id AND ".
						"type=$type AND seller_id={$this->uid} AND ".
						"buyer_id={$row->buyer_id}"
				);
				/**
				 * Das Geld zurückbuchen
				 */
				query(
					"UPDATE klunker SET amount=amount+{$row->price} ".
						"WHERE uid={$row->buyer_id}"
				);
			}
		}
		query("COMMIT");

		/**
		 * Test-Aktionen nicht loggen
		 */
		$log_db = new Db(Db::LOGS_HOST, Db::PAYMENT_NAME,
					Db::PAYMENT_PW, Db::LOGS_SELECT_DB);
		$log_db->query(
			"INSERT INTO item_trade_actions SET id=$id, type=$type, ".
				"seller_id={$this->uid}, buyer_id={$buyer_id}, price=$price, ".
				"timestamp=".Time::get_t_now()
		);
		// Ereignis-Log
		EventLog::get_instance($buyer_id)->add_entry(
			EventLog::TYPE_TRADE_ACCEPTED,
			$this->uid,
			"$type-$id-$price"
		);

		$result = new stdClass();
		$result->price = $price;
		$result->users_to_inform = $result_users_to_inform;
		return $result;
	}

	/**
	 * Schenkt dem User mit der ID $whom das durch $type und $id angegebene
	 * Objekt aus dem eigenen Besitz.
	 * @param uint $whom
	 * @param uint $type
	 * @param uint $id
	 * @return uint[2] 	Wieviele Items er derart noch besitzt:
	 * 	 array(ANZAHL_PRIVAT, ANZAHL_AUF_MARKTPLATZ)
	 * @throws IllegalArgumentException, Exception
	 */
	public function make_a_gift($whom, $type, $id) {
		$whom = (int)$whom;
		$type = (int)$type;
		$id = (int)$id;
		if ($whom <= 0) {
			throw new IllegalArgumentException("whom", "N+\{0}",$whom);
		}
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}

		query("START TRANSACTION");
		$res = query(	// Aktuell Anzahl (private, on_market_place) holen
			"SELECT amount, amount_mp FROM user_items ".
			"WHERE uid={$this->uid} AND type=$type AND id=$id"
		);
		if (mysql_num_rows($res) == 0) {
			query("ROLLBACK");
			throw new Exception("Not enough to give away.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
		list($amount, $amount_mp) = mysql_fetch_row($res);

		// Nun wird jeweils getestet, ob genug vorhanden ist. Zunächst die
		// Dinge, die auf dem Marktplatz sind
		if ($amount_mp > 0) {
			query (
				"UPDATE user_items SET amount_mp=amount_mp-1 ".
				"WHERE uid={$this->uid} AND id=$id AND type=$type"
			);
			$amount_mp--;
		}
		// Wenn davon keine da sind, dann von den privaten verschenken
		else if ($amount > 0) {
			query (
				"UPDATE user_items SET amount=amount-1 ".
				"WHERE uid={$this->uid} AND id=$id AND type=$type"
			);
			$amount--;
		}
		// Sind auchdavon keine da: Not enough to give away
		else {
			query("ROLLBACK");
			throw new Exception("Not enough to give away.",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}

		// Jetzt das Item dem anderen gutschreiben (abgezogen wurde eben schon)
		query (
			"INSERT INTO user_items SET uid=$whom, type=$type, id=$id, ".
				"amount=1 ".
			"ON DUPLICATE KEY UPDATE amount=amount+1"
		);
		query("COMMIT");
		ClientServer::send_request(CONF_CALLBACK_URL.
			"?action=make_a_gift".
			"&uid={$this->uid}".
			"&type=$type".
			"&id=$id".
			"&to_uid=$whom",
			true,
			CONF_MAMMUN_AUTH_SECRET
		);

		// Ereignis protokollieren
		EventLog::get_instance($whom)->add_entry(
			EventLog::TYPE_GIFT_RECEIVED,
			$this->uid,
			"$type-$id"
		);

		return array($amount, $amount_mp);
	}

	/**
	 * Wirft ein Item weg.
	 * @param $type
	 * @param $id
	 * @throws IllegalArgumentException, Exception
	 */
	public function throw_item_away($type, $id) {
		$type = (int)$type;
		$id = (int)$id;
		if ($type <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}",$type);
		}
		if ($id <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}",$id);
		}

		query(
			"UPDATE user_items SET amount=amount-1 ".
			"WHERE uid={$this->uid} AND id=$id AND type=$type AND amount>0"
		);
		if (mysql_affected_rows() == 0) {
			throw new Exception("Item cannot thrown away!",
				self::ERR_ITEM_DOES_NOT_EXIST);
		}
	}

	/**
	 * Gibt zurück, wer gerade alles die Schatzkiste von dem User mit der
	 * angegebenen ID besucht (Rückgabe sind User-IDs).
	 *
	 * @return uint[]
	 */
	public static function get_visitors_of_chest($uid) {
		query(
			"DELETE FROM user_chest_visits WHERE t_stamp<".
				(Time::get_t_now() - self::LOCK_VISIT) // 15min
		);
		$res = query(
			"SELECT visitor FROM user_chest_visits WHERE uid={$uid}"
		);
		$result = array();
		while ($row = mysql_fetch_row($res))
			$result[] = $row[0];
		return $result;
	}


	/**
	 * "Ich" besuche eine Schatzkiste eines anderen. Dieser Zustand wird
	 *  vermerkt.
	 *
	 * @param uint $uid
	 */
	public function visit_chest($uid) {
		$uid = (int)$uid;
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+\{0}",$uid);
		}

		query(
			"INSERT INTO user_chest_visits SET uid=$uid, ".
				"visitor={$this->uid}, t_stamp=".Time::get_t_now()." ".
			"ON DUPLICATE KEY UPDATE t_stamp=".Time::get_t_now()
		);
	}

	/**
	 * "Ich" beende den Besuch bei einer Schatzkiste. Der Besuchszustand
	 * wird zurückgesetzt.

	 * @param uint $uid
	 */
	public function unvisit_chest($uid) {
		$uid = (int)$uid;
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+\{0}",$uid);
		}

		query(
			"DELETE FROM user_chest_visits WHERE uid=$uid ".
				"AND visitor={$this->uid}"
		);
	}

	/**
	 * Markiert, dass "ich" kein Item mehr besuche - gar keins.
	 */
	public function unvisit_all() {
		query(
			"DELETE FROM user_chest_visits WHERE visitor={$this->uid}"
		);
	}

	/**
	 * Gibt ein assotiatives Array zurück, welches [type][id] auf den
	 * entsprechenden Einkaufspreis mappt.
	 *
	 * @return uint[][]
	 */
	public static function get_item_shop_price_table() {
		$res = query(
			"SELECT type, id, cost FROM items"
		);
		$result = null;
		while ($row = mysql_fetch_assoc($res)) {
			$result[$row['type']][$row['id']] = $row['cost'];
		}
		return $result;
	}
}
?>