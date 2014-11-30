<?php
require_once(dirname(__FILE__).'/IPOSTPayment.inc');
require_once(dirname(__FILE__).'/../shared/classes/ISO3166.inc');


class BOKU_Response_Parser {
	public static function get_instance() {
		if (!isset(self::$INSTANCE))
			self::$INSTANCE = new self();
		return self::$INSTANCE;
	}
	private static $INSTANCE;
	
	private function __construct() {}

	private static $result;
	private static $cur_key;

	/**
	 * Parset eine BOKU-XML-Antwort. Das Ergebnis ist ein assotiatives
	 * Array, dass prizipiell eindimensional ist. Im Falle von Doppelung
	 * von Tags wird diese Eindimensionalität zwangsläufig aufgebohrt,
	 * so dass für diesen Key ein Array entsteht mit allen gefundenen
	 * Vorkommen.
	 * 
	 * @param string $xml_data
	 * @return string[]
	 */
	public function parse($xml_data) {
		$xml_parser = xml_parser_create("UTF-8");
		xml_set_element_handler(
			$xml_parser,
			array($this, "start_elem"),
			array($this, "end_elem")
		);
		xml_set_character_data_handler(
			$xml_parser,
			array($this, "c_data")
		);
		xml_parse($xml_parser, trim($xml_data), true);
		xml_parser_free($xml_parser);
		return $this->result;
	}

	public function start_elem($parser, $tag, $attributes) {
		$this->cur_key = str_replace('-', '_', $tag);
		foreach ($attributes as $key => $value) {
		$key = str_replace('-', '_', $key);
			$k = $this->cur_key.".".$key;
			if (!isset($this->result[$k]))
				$this->result[$k] = $value;
			else {
				if (!is_array($this->result[$k])) {
					$tmp = $this->result[$k];
					$this->result[$k] = array();
					$this->result[$k][] = $tmp;
				}
				$this->result[$k][] = $value;
			}
		}
	}

	public function end_elem($parser, $tag) {
	}

	public function c_data($parser, $data) {
		if (trim($data) === '')
			return;
		if ($this->cur_key) {
			$this->result[$this->cur_key] = trim($data);
			$this->cur_key = null;
		}
    }
}


/**
 * Diese Klasse erlaubt Zahlung per Heidelpay-POST-Verfahren. Es wird ein Paket
 * per POST an Heidelpay gesandt, worauf eine URL als Antwort kommt, welche
 * vom Nutzer angesurft werden muss, um den Zahlungsvorgang abzuschließen.
 * 
 * @author Philipp Kemmeter
 */
class BOKUMobilePOST implements IPOSTPayment {
	private static $INSTANCES;
	private $uid;
	private $return_url;
	private $language;
	private $country;
	private $mobile_number;
	const SERVER_URL = "https://api2.boku.com/billing/request";
	const SERVICE_ID = "135389155cf0c115861fc3c8";
	const MERCHANT_ID = "gausus";
	const PASSWORD = "shad0wm4ster";
	const SECURITY_KEY = "mMPrga05BkSmxpY0ML1hfYZySgg6K8es08uq0R7j8fCVMfRcAynAn64UZUHvfmAPvqR0oJi259yME7LbbpuOAj9EADUb2ct3aRrr";
	
	
	/**
	 * Gibt die entsprechende Instanz zurück (Sigleton)
	 * @param uint $uid
	 * @return BOKUMobilePOST
	 */
	public static function get_instance($uid) {
		if (!isset(self::$INSTANCES[$uid]))
			self::$INSTANCES[$uid] = new self($uid);
		return self::$INSTANCES[$uid];
	}
	
	/**
	 * Erzeugt die entsprechende Instanz für den User mit der ID $uid.
	 * @param uint $uid
	 * @return BOKUMobilePOST
	 */
	private function __construct($uid) {
		$this->uid = $uid;
		
		//TODO Personal-Data-Access
		/*$user = User::get_instance_by_id($uid);
		$personal_data = $user->get_personal_data();
		$this->language = $personal_data->country 
							? strtoupper($personal_data->country)
							: "DE";
		$this->mobile_number = $personal_data->mobile;
		$this->country = strtolower($personal_data->country);*/
		$this->language = "DE";
	}
	
	/**
	 * Setzt die URL, die der User nach erfolgreicher Bezahlung angesurft 
	 * bekommt.
	 * 
	 * @param url $url
	 * @throws IllegalArgumentException
	 * @see include/classes/payment/IPOSTPayment#set_response_url()
	 */
	public function set_response_url($url) {
		if (strpos($url, "http://") !== 0) {
			throw new IllegalArgumentException(
				"\$url should start with http://, $url given.");
		}
		$this->return_url = $url;
	}
	
	/**
	 * Setzt das Land, aus dem der Anruf getätigt werden wird. Überschreibt
	 * somit ggf. im Profil des Users hinterlegte Daten.
	 * 
	 * @param string $country	(ISO 3166)
	 */
	public function set_country($country) {
		if (!in_array($country, ISO_3166::$all_codes))
			throw new IllegalArgumentException("country", "{ISO 3166}", $country);
		$this->country = $country;
	}
	
	/**
	 * Gibt die Preise zurück, die für den Markt möglich sind. Ist zudem
	 * $max_sms_num angegeben, werden nur die Preise zurückgegeben, die mit
	 * der angegebenen SMS Anzahl bezahlt werden können.
	 * @param string $country	(ISO 3166)
	 * @param uint $max_sms_num [default: INF]
	 * @return unknown_type
	 */
	public static function get_current_price_table($country, $max_sms_num=0,
		$payment_currency="EUR") {
		if (!in_array($country, ISO_3166::$all_codes))
			throw new IllegalArgumentException("country", "{ISO 3166}", $country);
		$post_array = array(
			'action' 				=> 'price',
			'merchant-id' 			=> self::MERCHANT_ID,
			'service-id'			=> self::SERVICE_ID,
			'country'				=> $country,
			'reference-currency'	=> $payment_currency,
			'timestamp'				=> Time::get_t_now()
		);
		error_log(print_r($post_array,true));
		$post_array['sig'] = self::create_signatur($post_array);
		$post_string = self::create_post_string($post_array);
		error_log(print_r($post_string,true));
		$result = self::send_post_package_to_server($post_string);
		error_log(print_r($result,true));
		if ($result["RESULT_CODE"] != 0) {
			throw new ExceptionWithErrorlog("Could not connect to BOKU!",
				"Unknown",	"BOKU: ".$result["RESULT_MSG"],
				$result["RESULT_CODE"]);
		}
		
		/**
		 * Wenn nur ein einziger Record exisitert, dann wird er nicht als Array
		 * dargelegt, sondern der eine Wert ist direkt darüber erreichbar. Das
		 * liegt am Parser und muss hier nun gesondert unterschieden werden.
		 */
		if (!is_array($result['PRICING.AMOUNT'])) {
			if ($max_sms_num && 
				$result["PRICING.NUMBER_BILLED_MESSAGES"] > $max_sms_num) 
			{
				return array();		// Keine Möglichkeit gefunden!
			}
			else {
				return array(
					$result["PRICING.AMOUNT"].'_'.$result['PRICING.CURRENCY']
						=>	$result['PRICING.REFERENCE_AMOUNT']
				);
			}
		}
		
		if ($max_sms_num > 0) {
			$n = count($result["PRICING.NUMBER_BILLED_MESSAGES"]);
			for ($i = 0; $i < $n; $i++) {
				if ($result["PRICING.NUMBER_BILLED_MESSAGES"][$i] > $max_sms_num)
					unset($result["PRICING.AMOUNT"][$i]);
			}
		}
		$return_value = null;
		foreach ($result["PRICING.AMOUNT"] as $key => $amount) {
			$return_value[$amount."_".$result["PRICING.CURRENCY"][$key]] 
				= $result["PRICING.REFERENCE_AMOUNT"][$key];
		}
		return $return_value;
	}
	
	/**
	 * Bei mobilen Anwendungen ist es nicht möglich, dass man den payment-amount
	 * frei wählt (aufgrund u.a. gesetzlicher Normen). Der gewünschte Payment-
	 * Amount, der hier angegeben wird, wird für das angegebene Land verglichen.
	 * Ist der Wert nicht verfügbar, wird der nächst höhere Wert zurückgegeben,
	 * welcher verfügbar ist. Ist kein höherer Wert verfügbar, wird der nächst
	 * niedrigere gewählt.
	 * 
	 * @param ufloat $payment_amount
	 * @param string $country	(ISO_3166)
	 * @param string $payment_currency
	 * @return ufloat
	 * @throws ExceptionWithErrorlog
	 */
	public static function correct_payment_amount($payment_amount, $country,
		$payment_currency='EUR')
	{
		if (!in_array($country, ISO_3166::$all_codes))
			throw new IllegalArgumentException("country", "{ISO 3166}", $country);
		
		error_log("Wanted amount: $payment_amount");
		
		$boku_amount_table = self::get_current_price_table($country);
		/**
		 * Das erhaltene Array wird nun durchsucht, um den Preis zu finden,
		 * der der nächst höhere Preis ist, falls der angeforderte nicht 
		 * verfügbar ist.
		 */
		$amount_key = $payment_amount*100;	// BOKU gibt Preise in Cent an
		
		/**
		 * Kein Array bedeutet, es gibt nur eine Möglichkeit. Diese zurückgeben.
		 */
		if (!is_array($boku_amount_table))
			return $boku_amount_table/100;
		else {
			/**
			 * Nun aufsteigend durchsuchen. Erster Hit der >= ist, ist der 
			 * nächste und somit gesuchte.
			 */
			sort($boku_amount_table);
			foreach ($boku_amount_table as $boku_amount) {
				if ($boku_amount >= $amount_key)
					return $boku_amount/100;
			}
			/**
			 * Wurde keiner gefunden, dann den höchst möglichen zurückgeben.
			 */
			return $boku_amount/100;
		}
		
	}
	
	/**
	 * ACHTUNG: Bei mobilen Anwendungen ist es NICHT möglich, dass man den
	 * payment-amount frei wählt! Der angegebene Wert wird entsprechend
	 * derart korrigiert, dass der nächst höhere Amount gewählt wird. Dieser
	 * wird dann zurückgeliefert, damit der Aufrufende der Funktion die
	 * Korrektur nachvollziehen kann.
	 * Um diese Korrektur vorzunehmen, muss zuvor das Land bekannt sein, aus
	 * dem der User anrufen möchte! Ansonsten gibt's ne Exception.
	 * 
	 * @return $ufloat
	 * @see include/classes/payment/IPOSTPayment#set_payment_information()
	 * @throws Exception
	 */
	public function set_payment_information($payment_amount, $payment_usage,
		$identification_transactionid, $payment_currency='EUR') 
	{
		if (!$this->country) {
			throw new ExceptionWithErrorlog(
				"BOKUMobilePOST::set_payment_information: NO COUNTRY!");
		}
		
		/**
		 * Zunächst den $payment_amount korrigieren.
		 */
		$new_amount = self::correct_payment_amount($payment_amount, 
				$this->country, $payment_currency);
		
		
		$this->payment_amount = $new_amount;
		$this->payment_currency = $payment_currency;
		$this->transaction_id = $identification_transactionid;
		$this->payment_usage = $payment_usage;
	}
	
	/**
	 * Diese Funktion generiert die Signatur für eine BOKU-Anfrage gemäß der
	 * Security-Spezifikation von BOKU (BOKU Security Implementation). Dazu
	 * muss ein Asso-Array von POST-Key-Value-Paren übergeben werden, welche
	 * an den BOKU-Server gesendet werden sollen. Auf der Basis der Variablen
	 * sowie des Security-Keys (self::SECURITY_KEY) wird dann ein md5-Hash
	 * erstellt, welcher als Parameter sig zusätzlich an den Server geschickt
	 * werden muss.
	 * Achtung: timestamp muss im übergebenen POST-Array enthalten sein.
	 * 
	 * @param string[] $post_array
	 * @return string
	 */
	private static function create_signatur($post_array) {
		if (!$post_array['timestamp'])
			throw new IllegalArgumentException("Kein timestamp in \$postarray: ".
													print_r($post_array,true));
		unset($post_array['password']);
		ksort($post_array);
		$result_string = '';
		foreach ($post_array as $key => $value) {
			if (empty($value))
				continue;
			$result_string .= "$key$value";
		}
		$result_string .= self::SECURITY_KEY;
		return md5($result_string);
		
	}
	
	/**
	 * Konvertiert ein Key-Value-Pair-Array in einen POST-String (&..=..&..=)
	 * 
	 * @param string[] $post_array
	 * @return string
	 */
	private static function create_post_string($post_array) {
		$result = '';
		foreach ($post_array as $key => $value)
			$result .= "$key=$value&";
		return substr($result, 0, -1);
	}
	
	
	private static function send_post_package_to_server($post_string) {
		$ch = curl_init();
		curl_setopt($ch, CURLOPT_URL, self::SERVER_URL);
		curl_setopt($ch, CURLOPT_VERBOSE, 1);
        
		// turning off the server and peer verification(TrustManager Concept).
		curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, FALSE);
		curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, FALSE);

		curl_setopt($ch, CURLOPT_RETURNTRANSFER,1);
		curl_setopt($ch, CURLOPT_POST, 1);
		error_log($post_string);
		// setting the $post_string as POST FIELD to curl
		curl_setopt($ch, CURLOPT_POSTFIELDS, $post_string);
		//getting response from server
		$response = curl_exec($ch);
		curl_close($ch);
		return BOKU_Response_Parser::get_instance()->parse($response);
	}
	
	/**
	 * @see include/classes/payment/IPOSTPayment#commit_post_payment()
	 */
	public function commit_post_payment() {
		$result_url = $this->return_url."?INVNUM={$this->transaction_id}".
											"&CUSTOM={$this->payment_usage}";
		$cancel_url = $this->return_url;
		$nvpreq="METHOD=SetExpressCheckout".
			"&VERSION=".urlencode(self::VERSION).
			"&PWD=".urlencode(self::PASSWORD).
			"&USER=".urlencode(self::USERNAME).
			"&SIGNATURE=".urlencode(self::SIGNATURE).
			"&EMAIL=".urlencode($this->email).
			"&AMT=".urlencode($this->payment_amount).
			"&CURRENCYCODE=".urlencode($this->payment_currency).
			"&RETURNURL=".urlencode($result_url).
			"&CANCELURL=".urlencode($cancel_url).
			"&GIROPAYSUCCESSURL=".urlencode($this->return_url).
			"&GIROPAYCANCELURL=".urlencode($this->return_url).
			"&PAYMENTACTION=Sale".
			"&INVNUM=".urlencode($this->transaction_id).
			"&CUSTOM=".urlencode($this->payment_usage).
			"&NOSHIPPING=1".
			"&DESC=".urlencode($this->payment_usage).
			"&LOCALCODE=".urlencode($this->language);

		return self::send_post_package_to_server($nvpreq);
	}
}
?>