<?php
@include_once(dirname(__FILE__).'/../_private_config.inc');

/**
 * So viele Klunker bekommt ein Spieler zu Beginn geschenkt
 */
define ('CONF_INITIAL_KLUNKER_AMOUNT',		0);

/**
 * Diese Items bekommt der User zu Beginn geschenkt
 */
define ('CONF_INITIAL_CLOTHES_0_TYPE',		1);
define ('CONF_INITIAL_CLOTHES_0_ID',		1);
/*define ('CONF_INITIAL_CLOTHES_1_TYPE',		2);
define ('CONF_INITIAL_CLOTHES_1_ID',		1);*/
// .
// .
// .
// define ('CONF_INITIAL_CLOTHES_N_TYPE',	X);
// define ('CONF_INITIAL_CLOTHES_N_ID',		Y);

if (!defined('CONF_CALLBACK_URL'))
	define ('CONF_CALLBACK_URL',
				'http://localhost:8123/game/payment_callback.php');

define ('CONF_MAMMUN_AUTH_SECRET', 	'293489f41898d18a53a6cf49ab476b96');
?>