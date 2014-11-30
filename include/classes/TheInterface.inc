<?php
require_once(dirname(__FILE__).'/Payment.inc');
require_once(dirname(__FILE__).'/ItemTrade.inc');
require_once(dirname(__FILE__).'/Statistics.inc');
require_once(dirname(__FILE__).
	'/../shared/classes/exceptions/IllegalArgumentException.inc');
require_once(dirname(__FILE__).'/../shared/classes/ValueChecker.inc');

/**
 * Die Schnittstelle zur Außenwelt :)
 *
 * @author Philipp Kemmeter
 */
class TheInterface {

	/**
	 * Diese Funktion schließt eine Klunker-Transaktions-Action ab. Wenn ein
	 * Nutzer extern bezahlt hat und nun seine Klunker dafür haben muss, müssen
	 * hier aus Sicherheitsgründen sowohl Produktname als auch Transaktions-ID
	 * angegeben werden.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param string $transaction_id
	 * @param string $product_id
	 * @throws IllegalArgumentException
	 * @throws ExceptionWithErrormail
	 * @throws HackattackException
	 * @return 1
	 */
	public static function cmd_finish_buy_klunker_transaction($params) {
		$uid = (int)$params[0];
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		$transaction_id = $params[1];
		$product_id = $params[2];
		Payment::get_instance($uid)->finish_buy_klunker_transaction(
			$transaction_id, $product_id
		);
		return 1;
	}

	/**
	 * Diese Funktion markiert in der Datenbank, dass eine Transaktion gestartet
	 * wurde und der User mit der angegebenen ID gerade dabei ist extern Geld
	 * zu bezahlen um hier Klunker zu bekommen.
	 * Gibt die ID zurück, unter der die Transaktion wieder zu finden ist.
	 * Dieser Rückgabewert ist die $transaction_id von
	 * cmd_finish_buy_klunker_transaction.
	 *
	 * @param uint $uid
	 * @return string
	 * @throws IllegalArgumentException
	 */
	public static function cmd_start_buy_klunker_transaction($uid) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		$uid = (int)$uid;
		return Payment::get_instance($uid)->start_buy_klunker_transaction();
	}

	/**
	 * Zieht dem Spieler mit der angegebenen ID entsprechend viele Klunker ab.
	 *
	 * Falls der User nicht genug hat, wird eine Exception geworfen.
	 *
	 * Gibt zurück. wie viele Klunker der User danach noch hat.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $klunker
	 * @param uint $purpose		Zum Tracken kann ein Grund angegeben werden.
	 * @return uint
	 */
	public static function substract_klunker($params) {
		$uid = (int)$params[0];
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N1", $uid);
		}
		$klunker = (int)$params[1];
		if ($klunker < 0) {
			throw new IllegalArgumentException("klunker", "N0", $klunker);
		}

		$purpose = (int)$params[2];
		if ($purpose < 0) {
			throw new IllegalArgumentException("purpose", "N0", $purpose);
		}

		return Payment::get_instance($uid)->substract_klunker(
			$klunker,
			$purpose
		);
	}

	/**
	 * Fügt dem Spieler mit der angegebenen ID entsprechend viele Klunker hinzu.
	 *
	 * Gibt zurück. wie viele Klunker der User danach hat.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $klunker
	 * @param uint $purpose
	 * @return uint
	 */
	public static function add_klunker($params) {
		if (!defined('CONF_UNIT_TEST_MODE') || !CONF_UNIT_TEST_MODE) {
			throw new Exception(
				"This functionality is available in unit test mode, only"
			);
		}
		$uid = ValueChecker::id((int)$params[0], 'uid');
		$klunker = ValueChecker::int((int)$params[1], 'klunker', 0);
		$purpose = ValueChecker::int((int)$params[2], 'purpose', 0);

		return Payment::get_instance($uid)->add_klunker(
			$klunker,
			$purpose
		);
	}

	/**
	 * Gibt die Anzahl Klunker des Users zurück.
	 *
	 * @param $uid
	 * @return uint
	 */
	public static function get_klunker_amount($uid) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		$uid = (int)$uid;
		return Payment::get_instance($uid)->get_klunker_amount();
	}

	/**
	 * Setzt die initialen Klunker und Items, die der User zu Beginn des Spiels
	 * geschenkt bekommt. Hat der Spieler bereits Klunker, wird der Wert NICHT
	 * verändert; dasselbe gilt für die Items.
	 * Somit ist diese Schnittstelle safe.
	 *
	 * @param uint $uid
	 */
	public static function init_user_klunker_and_items($uid) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		Payment::get_instance($uid)->init_klunker();
		ItemTrade::get_instance($uid)->init_items();
	}

	/**
	 * Gibt die Preistabelle für das Pricing zurück für das entsprechende Land
	 * von BOKU (Mobile Payment).
	 *
	 * @param string $country (ISO 3166)
	 * @return uint[]
	 * @throws IllegalArgumentException
	 */
	public static function get_boku_pricing_table($country) {
		return Payment::get_sms_price_table($country);
	}

	/**
	 * Setzt die Unique-ID für Heidelpaytransaktionen auf den angegebenen Wert
	 * für den User mit der ID $uid.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param string $unique_id
	 */
	public static function set_heidelpay_unique_id($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];
		$unique_id = $params[1];
		Payment::get_instance($uid)->set_heidelpay_unique_id($unique_id);
	}

	/**
	 * Gibt die Unique-ID für Heidelpaytransaktionen zurück.
	 *
	 * @param uint $uid
	 * @return string
	 */
	public static function get_heidelpay_unique_id($uid) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		$uid = (int)$uid;
		return Payment::get_instance($uid)->get_heidelpay_unique_id();
	}

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
	public static function statistics_get_all_user_items_amount($user_ids) {
		return Statistics::get_all_user_items_amount($user_ids);
	}

	/**
	 * Gibt alle Items von "mir" zurück. Es darf nach $type gefiltert werden.
	 *
	 * Die Rückgabe ist ein Array von stdClass:
	 *<code>
	 *  result[type][i]->id             ID des Items in Bezug auf $type
	 *  result[type][i]->amount         Wie viele ich davon habe und nicht anbiete
	 *  result[type][i]->amount_mp      Wie viele ich davon zum Verkauf anbiete
	 *  result[type][i]->type           Der Typ des Items
	 *  result[type][i]->shop_id        ID des Shops
	 *  result[type][i]->price          Der Preis, den ich mindestens dafür
	 *                                  haben will. Bei 0 muss ein Preis
	 *                                  ausgehandelt werden.
	 *  result[type][i]->days_in_shop   Wie lange der Schatz (angeblich) im Shop
	 *                                  war
	 *  result[type][i]->exclude_direct Wie viele davon ich nicht direkt
	 *                                  verkaufe
	 *  result[type][i]->shop_price     Preis, den das Item im Shop kostet
	 *  result[type][i]->locked         Gibt an, wie viele der Items auf dem
	 *                                  Marktplatz wegen Locks z.Z. nicht
	 *                                  verfügbar sind.
	 *  result[type][i]->offers = array();
	 *  result[type][i]->offers[j]->buyer_id  ID desjenigen, der Angebot gemacht
	 *  result[type][i]->offers[j]->price     Preis, den er geboten hat
	 *  result[type][i]->offers[j]->t_stamp   Zeitpunkt, wann Angebot gemacht
	 *</code>
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $type1 [=0 für alle typen]
	 * @param uint $type2
	 * ...
	 * @param uint $typeN
	 * @return stdClass[][]
	 */
	public static function get_my_items($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];
		$n = count($params);
		for ($i=1; $i<$n; $i++) {
			$result[$params[$i]]
				= ItemTrade::get_instance($uid)->get_my_items($params[$i]);
		}
		return isset($result[0]) ? $result[0] : $result;
	}

	/**
	 * Gibt alle Items der angegebenen Typen zurück, die man kaufen kann. Wird
	 * zudem $shop angegeben, kann danach gefiltert werden, in welchem Geschäft
	 * sich das Item befinden soll (entspricht der Preisklasse).
	 *
	 * Die Rückgabe ist ein Array von stdClass:
	 *
	 *<code>
	 *  result[type][i]->id         ID des Items in Bezug auf $type
	 *  result[type][i]->cost       Kosten des Items in Klunker
	 *                              (alle Rabatte inkl.)
	 *  result[type][i]->amount     Wie viele Items davon auf einmal gekauft
	 *                              werden müssen (Cost bezieht sich immer
	 *                              auf's Gesamtpaket)
	 *  result[type][i]->shop_id    In welchem Shop es das Item gibt
	 *  result[type][i]->d_discount Wie hoch der aktuelle, zeitlich beschränkte
	 *                              Rabatt ist
	 *  result[type][i]->q_discount Wie hoch der aktuelle Mengenrabatt ist
	 *  result[type][i]->available  Ob das Item aktuell verfügbar ist oder nicht
	 *  result[i]->av_until Bis wann das Item verfügbar bleibt. 0 heißt für
	 *                      immer, wenn avaiable==true, sonst keine Angabe.
	 *  result[i]->uid      ID eines exemplarischen Users, der das Item besitzt
	 *</code>
	 *
	 * @param IGNORE $params
	 * @param uint $shop [0 für alle]
	 * @param uint[] $types
	 * @param uint[] $uids
	 * @return stdClass[][]
	 */
	public static function get_all_items($params) {
		$shop = (int)$params[0];
		$types = $params[1];
		$uids = $params[2];
		if ($types && is_array($types)) {
			$n = count($types);
			for ($i = 0; $i < $n; $i++) {
				$result[$types[$i]]
					= ItemTrade::get_all_items($types[$i], $shop, $uids);
			}
			return $result;
		}
		return null;
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
	 *
	 * Die Rückgabe ist ein Array von stdClass-Objekten:
	 *<code>
	 *  result[type][i]->id         ID des Items in Bezug auf $type
	 *  result[type][i]->price      Preis des Items in Klunker
	 *  result[type][i]->amount     Wie viele Items der User davon
	 *  result[type][i]->shop_id    In welchem Shop es das Item normalerweise
	 *                              gibt
	 *  result[type][i]->uid        Die ID des Verkäufers
	 *</code>
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $shop [0 für alle]
	 * @param uint $limit
	 * @param bool $lock_items_for_user
	 * @param uint $type1
	 * @param uint $type2
	 * ...
	 * @param uint $typeN
	 * @return stdClass[][]
	 */
	public static function get_all_direct_trade_items($params) {
		if ($params[0] < 0) {
			throw new IllegalArgumentException("uid", "N", $params[0]);
		}
		$uid = (int)$params[0];
		if ($params[1] < 0) {
			throw new IllegalArgumentException("shop", "N", $params[1]);
		}
		$shop = (int)$params[1];
		if ($params[2] < 0) {
			throw new IllegalArgumentException("limit", "N", $params[2]);
		}
		$limit = (int)$params[2];
		$lock_items_for_user = (bool)$params[3];
		$n = count($params);
		for ($i=4; $i<$n; $i++) {
			if ($params[$i] <= 0) {
				throw new IllegalArgumentException("type".($i-3), "N+", $params[$i]);
			}
			$result[(int)$params[$i]]
				= ItemTrade::get_instance($uid)
					->get_all_direct_trade_items((int)$params[$i],
						$shop, $limit, $lock_items_for_user);
		}
		return $result;
	}

	/**
	 * Diese Funktion löst alle Sperren, die der User mit der angegebenen $uid
	 * ausgelöst hat.
	 *
	 * @param uint $uid
	 * @return 1	Kompatibilität. Immer 1.
	 */
	public static function release_item_locks($uid) {
		if ($uid < 0) {
			throw new IllegalArgumentException("uid", "N", $uid);
		}
		$uid = (int)$uid;
		ItemTrade::get_instance($uid)->release_all_locks();
		return 1;
	}

	/**
	 * Gibt die Items zurück, die aus den Schatzkisten der Spieler sind und
	 * im Shop angeboten werden, weil es dieses Item im Shop ausverkauft ist.
	 *
	 * Das Ergebnis ist geteilt in Trade-Items und Direct-Items (ersete zum
	 * Bieten, zweitere haben einen Mindestpreis und können direkt gekauft
	 * werden). Trade-Items sind zufällig gelost, Direct-Items sind welche, die
	 * am wenigesten Besucher haben. Pro Gruppe werden $limit zurückgegeben.
	 *
	 * Ergebnis:
	 *
	 *<code>
	 *  $result->trade[i]->uid       Wem das Item gehört
	 *  $result->trade[i]->amount    Wieviele davon noch zum Handel stehen
	 *  $result->trade[i]->visitors  Wieviele Nutzer die Schatzkiste gerade
	 *                               anschauen
	 *
	 *  $result->direct[i]->uid      Wem das Item gehört
	 *  $result->direct[i]->price    Angegebener Mindestpreis
	 *  $result->direct[i]->amount   Wieviele davon noch direkt gekauft werden
	 *                               können
	 *  $result->direct[i]->visitors Wieviele Nutzer die Schatzkiste gerade
	 * 								 anschauen
	 *</code>
	 * @param IGNORE $params
	 * @param uint $type
	 * @param uint $id
	 * @param uint $limit
	 * @param uint[] $include_only_uids	Nur diese User werden zurückgegeben
	 * @param uint[] $exclude_uids		Diese User werden ausgeschlossen
	 * @return stdClass
	 */
	public static function get_shop_alternative_chest_items($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("type", "N+\{0}", $params[0]);
		}
		$type = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("id", "N+\{0}", $params[1]);
		}
		$id = (int)$params[1];

		$limit = (int)$params[2];

		$result = new stdClass();
		$result->trade = ItemTrade::get_all_chest_trade_instances_of_item(
			$type, $id, $limit, (array)$params[3], (array)$params[4]
		);
		$result->direct = ItemTrade::get_all_chest_direct_instances_of_item(
			$type, $id, $limit, (array)$params[3], (array)$params[4]
		);
		return $result;
	}

	/**
	 * Gibt die Gesamtsumme aller Angebote für alle Items des Users zurück.
	 *
	 * @return uint
	 */
	public static function get_my_offers_amount_total($uid) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		$uid = (int)$uid;

		return ItemTrade::get_instance($uid)->get_my_offers_amounts(false);
	}

	/**
	 * Gibt alle Ausgehenden Angebote des Users zurück.
	 *
	 * @param uint $uid
	 * @return stdClass[]
	 */
	public static function get_outgoing_offers($uid) {
		return ItemTrade::get_instance($uid)->get_my_outgoing_offers();
	}

	/**
	 * Setzt die Eigenschaften eines Items in der Schatzkiste eines Users.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $type
	 * @param uint $id
	 * @param bool $sellable
	 * @param uint $min_price
	 * @param uint $exclude_direct
	 */
	public static function set_chest_item_props($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];
		if ($params[1] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[1]);
		}
		$type = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[2]);
		}
		$id = (int)$params[2];

		$sellable = !!$params[3];

		if ($params[4] < 0) {
			throw new IllegalArgumentException("min_price", "N", $params[4]);
		}
		$min_price = (int)$params[4];

		if ($params[5] < 0) {
			throw new IllegalArgumentException("exclude_direct", "N", $params[5]);
		}
		$exclude_direct = (int)$params[5];
		$item_trade = ItemTrade::get_instance($uid);
		if ($sellable) {
			$amount_array = $item_trade->put_items_on_mp($type, $id, 0);
		}
		else {
			$amount_array = $item_trade->remove_item_from_mp($type, $id, 0);
		}
		$item_trade->set_minimum_price($type, $id, $min_price, $exclude_direct);
		return $amount_array;
	}

	/**
	 * Stellt das durch Typ und ID identifizierte Item auf den virtuellen
	 * Marktplatz.
	 *
	 * Gibt ein Array zurück:
	 * $result[0] = ANZAHL_IN_KISTE_NICHT_AUF_MP
	 * $result[1] = ANZAHL_AUF_MP
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $type
	 * @param uint $id
	 * @return uint[2]
	 * @throws IllegalArgumentException
	 */
	public static function put_on_mp($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];
		if ($params[1] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[1]);
		}
		$type = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[2]);
		}
		$id = (int)$params[2];
		return ItemTrade::get_instance($uid)->put_items_on_mp($type,$id);
	}

	/**
	 * Entfernt das durch Typ und ID identifizierte Item vom virtuellen
	 * Marktplatz.
	 *
	 * Gibt ein Array zurück:
	 * $result[0] = ANZAHL_IN_KISTE_NICHT_AUF_MP
	 * $result[1] = ANZAHL_AUF_MP
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $type
	 * @param uint $id
	 * @return uint[2]
	 * @throws IllegalArgumentException
	 */
	public static function remove_from_mp($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];
		if ($params[1] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[1]);
		}
		$type = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[2]);
		}
		$id = (int)$params[2];
		return ItemTrade::get_instance($uid)->remove_item_from_mp($type,$id);
	}

	/**
	 * Diese Funktion verbraucht die angegebenen Kleider, wenn sie in der
	 * eigenen Schatzkiste sind. Sind sie nicht vorhanden, aber $buy_if_na ist
	 * gesetzt (default), dann werden die fehlenden Kleider direkt von uns
	 * gekauft. Können nicht alle kleider verbraucht werden, wird ein Fehler
	 * ausgeworfen und KEIN EINZIGES KLEIDUNGSSTÜCK wird verbraucht.
	 * Es wird die aktuell verbleibende Klunkeranzahl des Users zurückgegeben
	 * (dadurch kann geprüft werden, ob ein Kauf stattgefunden hat).
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param Array(type=>id) $items
	 * @param bool $buy_if_na=true
	 * @throws Exception
	 * @throws IllegalArgumentException
	 * @return uint
	 */
	public static function cmd_use_items($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];
		$items = $params[1];

		$buy_if_na = (isset($params[2])) ? $params[2] : true;
		ItemTrade::get_instance($uid)->use_items($items, $buy_if_na);
		return self::get_klunker_amount($uid);
	}



	/**
	 * Es wird versucht dem User ein Item direkt abzukaufen. Das funktioniert
	 * nur dann, wenn der Verkäufer mindestens ein solches Item zum
	 * Direktverkauf freigegeben hat und der Käufe (logo) genug Geld hat, um
	 * es zu kaufen.
	 * Da die Klunker unmittelbar abgezogen werden, wird zurückgegeben, wie
	 * viele Klunker der User nach der Aktion noch hat.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $seller_id
	 * @param uint $price		Preis, den der Käufer erwartet
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 */
	public static function buy_directly_from_user($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("seller_id", "N+", $params[1]);
		}
		$seller_id = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("price", "N+", $params[2]);
		}
		$price= (int)$params[2];

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[3]);
		}
		$type = (int)$params[3];

		if ($params[4] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[4]);
		}
		$id = (int)$params[4];

		$item_trade = ItemTrade::get_instance($uid);
		$result = new stdClass();
		$result->kkr = $item_trade
			->buy_directly_from_user($seller_id, $price, $type, $id);

		$result->item_amount = $item_trade->get_my_items_amount();
		return $result;
	}

	/**
	 * Mithilfe dieser Funktion kann man ein Item kaufen. Die Klunker, die das
	 * Item kostet, werden entsprechend vom Konto abgezogen, das Item dem User
	 * zugewiesen.
	 * Gibt ein stdObject zurück:
	 * <code>
	 *  $result->cost:          Wieviel das Item gekostet hat
	 *  $result->klunker_left:  Wieviele Klunker der User jetzt noch hat
	 * </code>
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $type
	 * @param uint $id
	 * @param uint $amount	[default: 1]
	 * @return uint[2]
	 * @throws Exception
	 * @throws IllegalArgumentException
	 */
	public static function cmd_buy_item($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[1]);
		}
		$type = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[2]);
		}
		$id = (int)$params[2];

		if (!isset($params[3]))
			$params[3] = 1;

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("amount", "N+", $params[3]);
		}
		$amount = (int)$params[3];

		$item_trade = ItemTrade::get_instance($uid);
		$result = new stdClass();
		$result->cost = $item_trade->buy_item(
			$type, $id, $amount
		);
		$result->klunker_left = self::get_klunker_amount($uid);
		$result->item_amount = $item_trade->get_my_items_amount();
		return $result;
	}

	/**
	 * Der User mit der UID <code>$uid macht</code> dem mit der UID
	 * <code>$seller_id</code> das Angebot, ein Item identifiziert durch
	 * <code>$id</code> und <code>$type</code> für <code>$price</code> Klunker
	 * abzukaufen.
	 *
	 * Gibt zurück, wie viele Klunker der User nach der Transaktion noch hat.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $seller_id
	 * @param uint $price
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 * @throws IllegalArgumentException
	 * @throws Exception
	 * @throws ExceptionWithErrorlog
	 */
	public static function make_offer($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("seller_id", "N+", $params[1]);
		}
		$seller_id = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("price", "N+", $params[2]);
		}
		$price= (int)$params[2];

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[3]);
		}
		$type = (int)$params[3];

		if ($params[4] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[4]);
		}
		$id = (int)$params[4];

		return ItemTrade::get_instance($uid)
			->make_offer($seller_id, $price, $type, $id);
	}

	/**
	 * Akzeptiert das Angebot, das von dem User mit der ID <code>$buyer_id</code>
	 * dem User
	 * mit der ID <code>$uid</code> gemacht wurde, und durch <code>$type</code>
	 * und <code>$id</code> genau spezifiert ist.
	 *
	 * Gibt eine Struktur folgender Form zurück:
	 * <code>
	 * $result->price                       Wie viele Klunker der Verkäufer
	 *                                      bekam ($uid) bekam
	 * $result->klunker                     Wie viele Klunker der Verkäufer nun
	 *                                      hat
	 * $result->users_to_inform => stdClass[] mit folgendem:
	 * $result->users_to_inform[i]->uid     ID des Users, der eine Mail bekommen
	 *                                      sollte, da sein Angebot automatisch
	 *                                      zurückgewiesen wurde
	 * $result->users_to_inform[i]->price   Anzahl der Klunker, die der User
	 *                                      zurückgebucht bekam
	 *</code>
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $buyer_id
	 * @param uint $type
	 * @param uint $id
	 * @return stdClass
	 * @throws IllegalArgumentException
	 * @throws Exception
	 */
	public static function accept_offer($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("buyer_id", "N+", $params[1]);
		}
		$buyer_id = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[2]);
		}
		$type = (int)$params[2];

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[3]);
		}
		$id = (int)$params[3];

		$result = ItemTrade::get_instance($uid)
			->accept_offer($buyer_id, $type, $id);
		$result->klunker = self::get_klunker_amount($uid);
		return $result;
	}

	/**
	 * Der User <code>$uid</code> zieht das eigene Angebot, das er dem User
	 * <code>$seller_id</code> gemacht hat und welches durch die Daten genau
	 * spezifiziert ist, zurück.
	 *
	 * Gibt den Preis zurück, der angeboten wurde.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $seller_id
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 */
	public static function withdraw_offer($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("seller_id", "N+", $params[1]);
		}
		$seller_id = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[2]);
		}
		$type = (int)$params[2];

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[3]);
		}
		$id = (int)$params[3];
		$result = new stdClass();
		$result->price = ItemTrade::get_instance($uid)
			->withdraw_offer($seller_id, $type, $id);
		$result->klunker = self::get_klunker_amount($uid);

		return $result;
	}

	/**
	 * User <code>$uid</code> lehnt ein von <code>$buyer_id</code> gemachtes
	 * Angebot ab, welches durch die Daten genau spezifiziert ist.
	 *
	 * Gibt den Preis zurück, der angeboten wurde
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $buyer_id
	 * @param uint $type
	 * @param uint $id
	 * @return uint
	 * @throws IllegalArgumentException
	 * @throws Exception
	 */
	public static function reject_offer($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("buyer_id", "N+", $params[1]);
		}
		$buyer_id = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("type", "N+", $params[2]);
		}
		$type = (int)$params[2];

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("id", "N+", $params[3]);
		}
		$id = (int)$params[3];

		return ItemTrade::get_instance($uid)
			->reject_offer($buyer_id, $type, $id);
	}

	/**
	 * Besucht die Schatzkiste eines Spielers.
	 *
	 * @param IGNORE $params
	 * @param uint $uid		Wer macht den Besuch
	 * @param uint $owner	Wessen Schatzkiste
	 * @return 1			Kompatibilität, immer 1
	 * @throws IllegalArgumentException
	 */
	public static function visit_chest($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		if ($params[1] <= 0) {
			throw new IllegalArgumentException("owner", "N+", $params[1]);
		}
		$uid = (int)$params[0];
		$owner = (int)$params[1];

		ItemTrade::get_instance($uid)->visit_chest($owner);
		return 1;
	}

	/**
	 * Beendet den Besuch der Schatzkiste eines Spielers.
	 *
	 * @param IGNORE $params
	 * @param uint $uid		Wer machte den Besuch
	 * @param uint $owner	Wessen Schatzkiste
	 * @return 1			Kompatibilität, immer 1
	 * @throws IllegalArgumentException
	 */
	public static function unvisit_chest($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		if ($params[1] <= 0) {
			throw new IllegalArgumentException("owner", "N+", $params[1]);
		}
		$uid = (int)$params[0];
		$owner = (int)$params[1];

		ItemTrade::get_instance($uid)->unvisit_chest($owner);
		return 1;
	}

	/**
	 * Gibt alle Besucher zurück (User-IDs) der Schatzkiste des angegebenen
	 * Users zurück.
	 *
	 * @param uint $uid		Wessen Schatzkiste
	 * @return uint[]
	 * @throws IllegalArgumentException
	 */
	public static function get_visitors_of_chest($uid) {
		if ($uid <= 0) {
			throw new IllegalArgumentException("uid", "N+", $uid);
		}
		$uid = (int)$uid;
		return ItemTrade::get_visitors_of_chest($uid);
	}

	/**
	 * Gibt den Befehl, alle Items des Users mit der angegebenen ID als nicht
	 * besucht zu markieren.
	 *
	 * @param uint $uid User-ID
	 * @return 1
	 */
	public static function unvisit_all_items($uid) {
		ItemTrade::get_instance($uid)->unvisit_all();
		return 1;
	}

	/**
	 * Gibt dem User die angegebenen Items (zurück) in die Schatzkiste.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint[] $item_obj
	 * @return 1
 	 * @throws IllegalArgumentException
	 */
	public static function put_items_into_chest($params) {
		if ($params[0] <= 0) {
			throw new IllegalArgumentException("uid", "N+", $params[0]);
		}
		$uid = (int)$params[0];

		ItemTrade::put_items_in_chest($uid, $params[1]);
		return 1;
	}

	/**
	 * Der User mit der ID <code>$uid</code> verschenkt ein durch
	 * <code>$type</code> und <code>$id</code> identifiziertes Item an den User
	 * mit der ID <code>$to_uid</code>.
	 *
	 * @param IGNORE $params
	 * @param uint $uid
	 * @param uint $type
	 * @param uint $id
	 * @param uint $to_uid
	 * @return uint[2] 	Wieviele Items er derart noch besitzt:
	 * 	 array(ANZAHL_PRIVAT, ANZAHL_AUF_MARKTPLATZ)
	 * @throws Exception
	 * @throws IllegalArgumentException
	 */
	public static function give_item_away($params) {
		if ($params[0] < 0) {
			throw new IllegalArgumentException("uid", "N0", $params[0]);
		}
		$uid = (int)$params[0];

		if ($params[1] <= 0) {
			throw new IllegalArgumentException("type", "N1", $params[1]);
		}
		$type = (int)$params[1];

		if ($params[2] <= 0) {
			throw new IllegalArgumentException("id", "N1", $params[2]);
		}
		$id = (int)$params[2];

		if ($params[3] <= 0) {
			throw new IllegalArgumentException("to_uid", "N1", $params[3]);
		}
		$to_uid = (int)$params[3];

		if ($uid == 0) {
			ItemTrade::give_user_item_from_null($to_uid, $type, $id);
			return array(0,0);
		}
		else
			return ItemTrade::get_instance($uid)
									->make_a_gift($to_uid, $type, $id);
	}

	/**
	 * Gibt ein assotiatives Array zurück, welches [type][id] auf den
	 * entsprechenden Einkaufspreis mappt.
	 *
	 * @return uint[][]
	 */
	public static function get_item_shop_price_table() {
		return ItemTrade::get_item_shop_price_table();
	}
}
?>