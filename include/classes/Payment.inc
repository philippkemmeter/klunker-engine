<?php
require_once(dirname(__FILE__).'/HeidelpayXML.inc');
require_once(dirname(__FILE__).'/../shared/classes/ISO3166.inc');
require_once(dirname(__FILE__).'/BOKUMobilePOST.inc');
require_once(dirname(__FILE__).'/Db.inc');

/**
 * Diese Klasse behandelt das Payment. Es wird eine virtuelle Wärung (Klunker)
 * pro Spieler verwaltet, sowie das Aufladen des Kontos, ausgeben der Klunker
 * usw. übernommen.
 *
 * @author Philipp Kemmeter
 */
class Payment {
	private $uid;

	const PAYMENT_METHOD_CREDIT_CARD = 0;
	const PAYMENT_METHOD_DIRECT_DEBIT = 1;
	const PAYMENT_METHOD_HANDY_PAY = 2;

	private static $INSTANCES;

	/**
	 * Gibt die für die übergebene User-ID definierte Instanz zurück.
	 * Erstellt, wenn nötig, eine neue Instanz (SIGLETON).
	 *
	 * @param uint $uid
	 * @return Payment
	 */
	public static function get_instance($uid) {
		if (!isset(self::$INSTANCES[$uid])) {
			self::$INSTANCES[$uid] = new self($uid);
		}
		return self::$INSTANCES[$uid];
	}

	/**
	 * Erstellt eine neue Payment-Instanz für die angegebenen $uid.
	 *
	 * @param uint $uid
	 * @return Payment
	 */
	private function __construct($uid) {
		$this->uid = $uid;
	}

	/**
	 * Gibt zurück, wie viele Klunker der User aktuell besitzt.
	 *
	 * @return uint
	 */
	public function get_klunker_amount() {
		return (int)(@mysql_result(
			query(
				"SELECT amount FROM klunker WHERE uid={$this->uid}"
			),0
		));
	}

	/**
	 * Zieht dem Spieler die angegbenen Klunker ab.
	 *
	 * Wenn nicht genug Klunker, dann Exception
	 *
	 * Gibt zurück. wie viele Klunker der User danach noch hat.
	 *
	 * @param uint $amount
	 * @param uint $purpose
	 * @return uint
	 * @throws Exception
	 */
	public function substract_klunker($amount, $purpose) {
		query("START TRANSACTION");
		$old_amt = $this->get_klunker_amount();
		if ($old_amt < $amount) {
			query("ROLLBACK");
			throw new Exception("Not enough Klunker");
		}
		$new_amt = $old_amt - $amount;
		query(
			"INSERT INTO klunker SET amount=$new_amt, uid={$this->uid} ".
			"ON DUPLICATE KEY UPDATE amount=$new_amt"
		);
		query("COMMIT");
		$log_db = new Db(Db::LOGS_HOST,Db::LOGS_NAME,
								Db::LOGS_PW, Db::LOGS_SELECT_DB);
		$log_db->query(
			"INSERT INTO klunker_substract_actions SET type=$purpose, ".
				"uid={$this->uid}, amount=$amount, ".
				"timestamp=".Time::get_t_now()
		);
		return $new_amt;
	}

	/**
	 * Fügt dem Spieler mit der angegebenen ID entsprechend viele Klunker hinzu.
	 *
	 * Gibt zurück. wie viele Klunker der User danach hat.
	 *
	 * @param uint $amount
	 * @param uint $purpose
	 * @return uint
	 */
	public function add_klunker($amount, $purpose) {
		query("START TRANSACTION");
		query(
			"INSERT INTO klunker SET amount=$amount, uid={$this->uid} ".
			"ON DUPLICATE KEY UPDATE amount=amount+$amount"
		);
		$result = $this->get_klunker_amount();
		query("COMMIT");

		return $result;
	}

	/**
	 * Setzt die initiale Klunkeranzahl. Besitzt der
	 * Spieler bereits Klunker, oder hat er mal welche besessen, passiert hier
	 * genau nichts. Somit ist diese Schnittstelle safe, da sie keine
	 * Möglichkeit zur Klunkermanipulation bietet.
	 *
	 */
	public function	init_klunker() {
		query(		/** Das IGNORE bringt hier die Sicherheit */
			"INSERT IGNORE INTO klunker SET amount=".CONF_INITIAL_KLUNKER_AMOUNT
			.", uid={$this->uid}"
		);
	}

	/**
	 * Gibt die aktuellen Mobilfunk-SMS-Preise (inklusive
	 * Mehrfach-SMS-Buchungen) als ein Array zurück (in cent).
	 *
	 * @param string $country	(ISO 3166)
	 * @return uint[]
	 */
	public static function get_sms_price_table($country) {
		if (!in_array($country, ISO_3166::$all_codes))
			throw new IllegalArgumentException("country", "{ISO 3166}", $country);

		/**
		 * Zunächst wird herausgefunden, ob die Daten in der DB noch aktuell
		 * sind.
		 */
		$res = query(
			"SELECT t_stamp FROM sms_payment_prices_tstamp ".
			"WHERE country='$country'"
		);
		$t_stamp = @mysql_result($res,0);
		if (!$t_stamp || ($t_stamp < (Time::get_t_now() - 86400 /*24h*/))) {
			/**
			 * Wenn veraltete Werte in der Datenbank stehen, dann von BOKU die
			 * aktuellen anfordern und die neuen Werte in die DB speichern.
			 */
			$price_table = BOKUMobilePOST::get_current_price_table($country, 1);
			query("START TRANSACTION");
			query("DELETE FROM sms_payment_prices WHERE country='$country'");
			foreach ($price_table as $amount_w_currency => $ref_amount) {
				list($amount_c, $currency) = explode("_",$amount_w_currency);
				if (!$amount_c || !$currency || !$ref_amount)
					continue;
				query(
					"INSERT INTO sms_payment_prices SET ".
						"country='$country', reference_price=$ref_amount, ".
						"price=$amount_c, currency='$currency'"
				);
			}
			query(
				"INSERT INTO sms_payment_prices_tstamp SET ".
					"t_stamp=".Time::get_t_now().", country='$country'".
				"ON DUPLICATE KEY UPDATE t_stamp=".Time::get_t_now()
			);
			query("COMMIT");
		}
		else {
			$price_table = array();
			$res = query(
				"SELECT price, reference_price, currency ".
					"FROM sms_payment_prices WHERE country='$country'"
			);
			while ($row = mysql_fetch_assoc($res)) {
				$price_table[$row['price'].'_'.$row['currency']]
					= $row['reference_price'];
			}
		}
		return $price_table;
	}

	/**
	 * Setzt die Unique-ID für Heidelpaytransaktionen auf den angegebenen Wert.
	 *
	 * @param string $unique_id
	 */
	public function set_heidelpay_unique_id($unique_id) {
		query(
			"INSERT INTO unique_ids SET uid={$this->uid}, ".
				"heidelpay='$unique_id' ".
			"ON DUPLICATE KEY UPDATE heidelpay='$unique_id'"
		);
	}

	/**
	 * Gibt die Unique-ID für Heidelpaytransaktionen zurück.
	 *
	 * @return string
	 */
	public function get_heidelpay_unique_id() {
		$res = query(
			"SELECT heidelpay FROM unique_ids WHERE uid={$this->uid}"
		);
		return @mysql_result($res,0);
	}

	/**
	 * Diese Funktion markiert in der Datenbank, dass eine Transaktion gestartet
	 * wurde und der User mit der angegebenen ID gerade dabei ist extern Geld
	 * zu bezahlen um hier Klunker zu bekommen.
	 * Gibt die ID zurück, unter der die Transaktion wieder zu finden ist.
	 * Dieser Rückgabewert ist die $transaction_id von
	 * Payment::finish_buy_klunker_transaction.
	 *
	 * @return string
	 * @see include/classes/payment/Payment#finish_buy_klunker_transaction()
	 */
	public function start_buy_klunker_transaction() {
		query("START TRANSACTION");
		do {
			$transaction_id = rand(0, 16777215);	// 24Bit
			$res = query(
				"SELECT COUNT(*) FROM transaction_ids ".
				"WHERE transaction_id=$transaction_id"
			);
		} while (mysql_result($res,0) > 0);

		query(
			"INSERT INTO transaction_ids SET uid={$this->uid}, ".
				"transaction_id=$transaction_id, t_stamp=".Time::get_t_now()
		);
		query("COMMIT");
		return $transaction_id;
	}

	/**
	 * Diese Funktion schließt eine Klunker-Transaktions-Action ab. Wenn ein
	 * Nutzer extern bezahlt hat und nun seine Klunker dafür haben muss, müssen
	 * hier aus Sicherheitsgründen sowohl Produktname als auch Transaktions-ID
	 * angegeben werden.
	 *
	 * @param string $transaction_id
	 * @param string $product_id
	 * @see include/classes/payment/Payment#start_buy_klunker_transaction()
	 */
	public function finish_buy_klunker_transaction($transaction_id,
		$product_id)
	{
		query("START TRANSACTION");
		/**
		 * Zunächst alle Transaktions-IDs löschen, die älter als 3 Tage sind.
		 */
		query(
			"DELETE FROM transaction_ids WHERE t_stamp<".
				(Time::get_t_now()-10800)
		);
		$res = query(
			"SELECT t_stamp FROM transaction_ids WHERE uid={$this->uid} AND ".
				"transaction_id=$transaction_id"
		);
		if (mysql_num_rows($res) == 0) {
			query("ROLLBACK");
			throw new HackattackException("Transaction error!",$this->uid,
				HackattackException::CODE_TRANSACTION_ID_WRONG);
		}
		$t_stamp = mysql_result($res,0);

		query(
			"DELETE FROM transaction_ids WHERE uid={$this->uid} AND ".
				"transaction_id=$transaction_id"
		);

		list($junk, $amount_to_add) = explode("_", $product_id);
		query(
			"INSERT INTO klunker SET amount=$amount_to_add, ".
				"uid={$this->uid} ".
			"ON DUPLICATE KEY UPDATE amount=amount+$amount_to_add"
		);
		query("COMMIT");
	}

	/**
	 * Der User kauft hierüber Klunker durch Angabe der Zahlungsmethode ($method),
	 * der Euro-Menge, die er ausgeben möchte ($spent_euro) und den
	 * Transaktionsdetails ($details).
	 *
	 * Die Zahlungsmethode muss eine der Klassenkonstanten ::PAYMENT_METHOD_...
	 * sein. Die Transaktionsdetails sind von der gewählten Methode abhängig zu
	 * wählen.
	 *
	 * Bei Kreditkartenzahlen muss $details wie folgt aussehen:
	 * $details->account_holder			Kontoinhaber
	 * $details->account_brand			Kreditkartenfirma (bsp. VISA)
	 * $details->account_number			Kreditkartennummer
	 * $details->account_verification	Verifikationscode
	 * $details->account_month			Monat des Ablaufdatums der Karte
	 * $details->account_year			Jahr des Ablaufdatums der Karte
	 *
	 * Bei Direkter-Kontotransaktion (Direct Debit) muss $details so aussehen:
	 * $details->account_holder			Kontoinhaber
	 * $details->account_number			Kontonummer
	 * $details->account_bankname		Bankleitzahl
	 * $details->account_country		ISO 3166 Ländercode (z.B. DE. s.
	 * 										ISO3166.inc)
	 *
	 * Bei Handypay muss $details wie folgt aussehen:
	 * $details->contact_mobile			Mobilfunkrufnummer
	 *
	 * @param uint $method
	 * @param ufloat $spent_euro
	 * @param stdClass $details
	 * @throws Exception
	 */
	public function buy_klunker($method, $spent_euro, $details) {
		if (!isset(self::$euro_to_klunker[$method][$spent_euro])) {
			throw new IllegalArgumentException(
				"For the given Method is the given euro amount not available. ".
					"\$method: $method, \$spent_euro: $spent_euro"
			);
		}
		$klunker_to_add = self::$euro_to_klunker[$method][$spent_euro];

		switch ($method) {
			/*
			 * XML-Payment nicht erlaubt von Heidelpay!
			 * => Das wurde also auch noch nie getestet.
			 *
			case self::PAYMENT_METHOD_CREDIT_CARD:
				$heidelpay = HeidelpayXML::get_instance($this->uid);
				$heidelpay->set_account_information(
					$details->account_holder,
					$details->account_brand, $details->account_number,
					'', '', '', $details->account_verification,
					$details->account_year, $details->acount_month
				);
				$heidelpay->set_payment_code(
					HeidelpayXML::PAYMENT_CODE_CREDITCARD
				);
				$heidelpay->set_payment_information(
					$spent_euro, "Klunker",	"My ID", "EUR"
				);
				$heidelpay->commit_XML_payment();
				break;
			case self::PAYMENT_METHOD_DIRECT_DEBIT:
				$heidelpay = HeidelpayXML::get_instance($this->uid);
				$heidelpay->set_account_information(
					$details->account_holder,
					'', $details->account_number, $details->account_bankname,
					$details->account_country, '', '', '', ''
				);
				$heidelpay->set_payment_code(
					HeidelpayXML::PAYMENT_CODE_DIRECTDEBIT
				);
				$heidelpay->set_payment_information(
					$spent_euro, "Klunker", "My ID", "EUR"
				);
				$heidelpay->commit_XML_payment();
				break;
			*/
			case self::PAYMENT_METHOD_HANDY_PAY:
				break;
			default:
				throw new IllegalArgumentException("method",
					"[".self::PAYMENT_METHOD_CREDIT_CARD.",".
						self::PAYMENT_METHOD_DIRECT_DEBIT.",".
						self::PAYMENT_METHOD_HANDY_PAY."]", $method);
		}
		query(
			"INSERT INTO klunker SET amount=$klunker_to_add, uid={$this->uid} ".
			"ON DUPLICATE KEY UPDATE amount=amount+$klunker_to_add"
		);
	}
}
?>