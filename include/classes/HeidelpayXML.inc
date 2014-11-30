<?php
/**
 * Diese Klasse verwaltet das Aufladen des Klunkerkontos per Heidelpay.
 * 
 * @author Philipp Kemmeter
 */
class HeidelpayXML {
	const SENDER_ID = 'd225a9fefe3fbaf400fe43281064000a';
	const CHANNEL_ID = 'd225a9fefe3fbaf400fe43294aca000d';
	const USER_ID = '8a90818a230893b201230f089dab03c1';
	const PASSWORD = 'demo';
	const SERVER = 'test.ctpe.net';
	const PATH = '/payment/ctpe';
	
	const TRANSACTION_MODE = 'INTEGRATOR_TEST';
	const TRANSACTION_RESPONSE = 'SYNC';
	
	const PAYMENT_CODE_CREDITCARD = 0;
	const PAYMENT_CODE_DIRECTDEBIT = 1;
	const REQUEST_VERSION = 1.0;
	
	private $user_agent = "php ctpepost";
	
	private $uid;
	
	private $identification_transactionid;
	private $account_holder;
	private $account_brand;
	private $account_number;
	private $account_bankname;
	private $account_country;
	private $account_authorisation;
	private $account_verification;
	private $account_year;
	private $account_month;
	
	private $payment_code;
	private $payment_amount;
	private $payment_usage;
	private $payment_currency;
	
	private $contact_mobile;
	private $contact_phone;
	private $contact_email;
	private $contact_ip;
	
	private $address_street;
	private $address_zip;
	private $address_city;
	private $address_state;
	private $address_country;
	
	private $name_salutation;
	private $name_title;
	private $name_give;
	private $name_family;
	private $name_company;
	
	
	
	private static $INSTANCES;
	
	/**
	 * Gibt die entsprechende Instanz zurück.
	 * 
	 * @param uint $uid
	 * @return HeidelpayXML
	 */
	public static function get_instance($uid) {
		if (!isset(self::$INSTANCES[$uid]))
			self::$INSTANCES[$uid] = new self($uid);
		return self::$INSTANCES[$uid];
	}
	
	/**
	 * Erstellt eine neue Instanz für User $uid.
	 * @param uint $uid
	 * @return HeidelpayXML
	 */
	private function __construct($uid) {
		$this->uid = $uid;
		
		$user = User::get_instance_by_id($uid);
		$personal_data = $user->get_personal_data();
		
		
		$this->contact_email = $user->get_email();
		$this->contact_ip = $_SERVER['REMOTE_ADDR'];
		$this->contact_phone = $personal_data->phone;
		$this->contact_mobile = $personal_data->mobile;
		
		$this->name_salutation = $personal_data->salutation;
		$this->name_title = $personal_data->title;
		$this->name_give = $personal_data->firstnames;
		$this->name_family = $personal_data->surnames;
		$this->name_company = '';
		
		$this->address_street = $personal_data->street . " " . 
									$personal_data->street_number;
		$this->address_city = $personal_data->city;
		$this->address_state = $personal_data->state;
		$this->address_country = $personal_data->country;
		$this->address_zip = $personal_data->zipcode;
	}

	/**
	 * Sendet alle Daten zu CTPE.
	 * 
	 * @param string $host
	 * @param string $path
	 * @param string $data
	 */
	private function sendToCTPE($host,$path,$data) {								   
		$cpt = curl_init(); 
		
		$xmlpost="load=".urlencode($data);
		curl_setopt($cpt, CURLOPT_URL, "https://$host$path");
		curl_setopt($cpt, CURLOPT_SSL_VERIFYHOST, 1);						  
		curl_setopt($cpt, CURLOPT_USERAGENT, $this->user_agent);			   
		curl_setopt($cpt, CURLOPT_RETURNTRANSFER, 1);						  
		//curl_setopt($cpt, CURLOPT_SSL_VERIFYPEER, FALSE);					
		curl_setopt($cpt, CURLOPT_SSL_VERIFYPEER, 1);						  
		
		curl_setopt($cpt, CURLOPT_POST, 1);									
		curl_setopt($cpt, CURLOPT_POSTFIELDS, $xmlpost);					   
		
		$this->resultURL = curl_exec($cpt);									
		$this->error = curl_error($cpt);									   
		$this->info = curl_getinfo($cpt);									  
	
		curl_close($cpt);													  
	}
	
	/**
	 * Setzt die Kontodaten
	 * 
	 * @param string $account_holder
	 * @param string $account_brand
	 * @param string $account_number
	 * @param string $account_bankname
	 * @param string $account_country
	 * @param string $account_authorisation
	 * @param string $account_verification
	 * @param string $account_year
	 * @param string $account_month
	 */
	public function set_account_information($account_holder, $account_brand,
		$account_number, $account_bankname, $account_country,
		$account_authorisation, $account_verification, $account_year,
		$account_month)											   
	{   
		$this->account_holder=$account_holder;								 
		$this->account_brand=$account_brand;
		$this->account_number=$account_number;
		$this->account_bankname=$account_bankname;							 
		$this->account_country=$account_country;
		$this->account_authorisation=$account_authorisation;				   
		$this->account_verification=$account_verification;					 
		$this->account_year=$account_year;
		$this->account_month=$account_month;								   
	}																		  
	
	/**
	 * Setzt den Paymentcode entsprechend auf Kreditkarte oder Giropay 
	 * (directdebit).
	 * 
	 * @param [0,1] $payment_code
	 */
	public function set_payment_code($payment_code)									 
	{   
		if ($payment_code == self::PAYMENT_CODE_CREDITCARD)
			$this->payment_code = 'CC.DB';
		else if ($payment_code == self::PAYMENT_CODE_DIRECTDEBIT)
			$this->payment_code = 'DD.CB';
		else throw new IllegalArgumentException(
				"payment_code", "[0,1]", $payment_code
			);
	}

	/**
	 * Setzt die Zahlungsinformationen.
	 * 
	 * @param ufloat $payment_amount		Wie viel Geld
	 * @param string $payment_usage			Was gekauft wurde
	 * @param string $identification_transactionid
	 * @param string $payment_currency		Währung (DEFAULT: EUR)
	 */
	public function set_payment_information($payment_amount, $payment_usage,
		$identification_transactionid, $payment_currency='EUR')
	{
		$this->payment_amount = (string)$payment_amount;
		$this->payment_usage = $payment_usage;
		$this->identification_transactionid = $identification_transactionid;
		$this->payment_currency = $payment_currency;
	}
	
	 
	/**
	 * Schickt das konfigurierte Payment Paket als XML ab zu Heidelpay (CTPE).
	 * 
	 * @throws Exception
	 */
	public function commit_XML_payment() {
		$strXML = "<?xml version=\"1.0\" ?>";
	
		// set account and user information.
		$strXML .= "<Request version=\"".self::REQUEST_VERSION."\">";
		$strXML .= "<Header>";
		$strXML .= "<Security sender=\"".self::SENDER_ID."\" token=\"token\" />";
		$strXML .= "</Header>";
		$strXML .= "<Transaction response=\"".self::TRANSACTION_RESPONSE."\" ".
						"channel=\"".self::CHANNEL_ID."\" ".
						"mode=\"".self::TRANSACTION_MODE."\">";
		$strXML .= "<User pwd=\"".self::PASSWORD."\" ".
						"login=\"".self::USER_ID."\" />";
		$strXML .= "<Identification>";
		$strXML .= "<TransactionID>{$this->identification_transactionid}</TransactionID>";
		$strXML .= "</Identification>";
	
		// set account information
	
		$strXML .= "<Account>";
		$strXML .= "<Holder>{$this->account_holder}</Holder>";
		$strXML .= "<Brand>{$this->account_brand}</Brand>";
		$strXML .= "<Number>{$this->account_number}</Number>";
		$strXML .= "<Bank>{$this->account_bankname}</Bank>";
		$strXML .= "<Country>{$this->account_country}</Country>";
		$strXML .= "<Authorisation>{$this->account_authorisation}</Authorisation>";
		$strXML .= "<Verification>{$this->account_verification}</Verification>";
		$strXML .= "<Year>{$this->account_year}</Year>";
		$strXML .= "<Month>{$this->account_month}</Month>";
		$strXML .= "</Account>";
	
		// set payment information
	
		$strXML .= "<Payment code=\"{$this->payment_code}\">";
		$strXML .= "<Presentation>";
		$strXML .= "<Amount>{$this->payment_amount}</Amount>";
		$strXML .= "<Usage>{$this->payment_usage}</Usage>";
		$strXML .= "<Currency>{$this->payment_currency}</Currency>";
		$strXML .= "</Presentation>";
		$strXML .= "</Payment>";
	
		// set customer information
	
		$strXML .= "<Customer>";
			$strXML .= "<Contact>";
		$strXML .= "<Email>{$this->contact_email}</Email>";
		$strXML .= "<Mobile>{$this->contact_mobile}</Mobile>";
		$strXML .= "<Ip>{$this->contact_ip}</Ip>";
		$strXML .= "<Phone>{$this->contact_phone}</Phone>";
		$strXML .= "</Contact>";
		$strXML .= "<Address>";
		$strXML .= "<Street>{$this->address_street}</Street>";
		$strXML .= "<Zip>{$this->address_zip}</Zip>";
		$strXML .= "<City>{$this->address_city}</City>";
		$strXML .= "<State>{$this->address_state}</State>";
		$strXML .= "<Country>{$this->address_country}</Country>";
		$strXML .= "</Address>";
		$strXML .= "<Name>";
		$strXML .= "<Salutation>{$this->name_salutation}</Salutation>";
		$strXML .= "<Title>{$this->name_title}</Title>";
		$strXML .= "<Given>{$this->name_give}</Given>";
		$strXML .= "<Family>{$this->name_family}</Family>";
		$strXML .= "<Company>{$this->name_company}</Company>";
		$strXML .= "</Name>";
		$strXML .= "</Customer>";
	
	
		$strXML .= "</Transaction>";
		$strXML .= "</Request>";
	
		$this->sendToCTPE(self::SERVER,self::PATH,$strXML);
	
		if ($this->resultURL)
		{
			return $this->parserResult($this->resultURL);
		}
		else
		{
			return false;
		}
	}
		
	/**
	 * Parser XML message returned by CTPE server.
	 * 
	 * @param string $resultURL
	 * @throws Exception
	 */
	private function parserResult($resultURL)
	{
		$resultXML=urldecode($resultURL);
		$processingResult = substr($resultXML, 
			strpos($resultXML, "<Result>")+strlen("<Result>"), 
			strpos($resultXML, "</Result>")-strlen("<Result>")
				-strpos($resultXML, "<Result>")
		);

		if ($processingResult == "ACK")
		{
			return $processingResult;
		}
		else
		{
			$errorCode = substr($resultXML,
				strpos($resultXML, "<Return code=")
						+strlen("<Return code=\"000.000.000\">"), 
				strpos($resultXML, "</Return>")
						-strlen("<Return code=\"000.000.000\">")
						-strpos($resultXML, "<Return code")
			);
			throw new Exception($errorCode);
		}
	}
}
?>