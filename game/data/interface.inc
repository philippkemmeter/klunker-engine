<?php
require_once ('../include/_config.inc');
require_once ('../include/classes/TheInterface.inc');
require_once ('../include/shared/classes/server_com/ClientServer.inc');
require_once ('../include/shared/classes/PV_encryption.inc');
require_once ('../include/shared/classes/Time.inc');
require_once ('../include/shared/classes/exceptions/ExceptionWithErrorlog.inc');
require_once ('../include/shared/classes/exceptions/ExceptionWithErrormail.inc');
$cryptObj = new PV_encryption();
$auth = TrustedServerAuth::verify($_REQUEST, CONF_MAMMUN_AUTH_SECRET);
if (!$auth) {
	echo "<b>Error</b>: Authentification failed!";
	die();
}

$expected_data = array (
	'cmd_start_buy_klunker_transaction'		=>	'(e)serialized',
	'cmd_finish_buy_klunker_transaction'	=>	'(e)serialized',
	'substract_klunker'						=>	'(e)serialized',
	'add_klunker'							=>	'(e)serialized',
	'get_klunker_amount'					=>	'(e)serialized',
	'init_user_klunker_and_items'			=>	'(e)serialized',
	'get_boku_pricing_table'				=>	'(e)serialized',
	'set_heidelpay_unique_id'				=>	'(e)serialized',
	'get_heidelpay_unique_id'				=>	'(e)serialized',
	'get_all_items'							=>	'(e)serialized',
	'get_all_direct_trade_items'			=>	'(e)serialized',
	'statistics_get_all_user_items_amount'	=>	'(e)serialized',
	'get_my_items'							=>	'(e)serialized',
	'cmd_use_items'							=>	'(e)serialized',
	'cmd_buy_item'							=>	'(e)serialized',
	'put_on_mp'								=>	'(e)serialized',
	'buy_directly_from_user'				=>	'(e)serialized',
	'set_chest_item_props'					=>	'(e)serialized',
	'remove_from_mp'						=>	'(e)serialized',
	'make_offer'							=>	'(e)serialized',
	'accept_offer'							=>	'(e)serialized',
	'reject_offer'							=>	'(e)serialized',
	'withdraw_offer'						=>	'(e)serialized',
	'get_outgoing_offers'					=>	'(e)serialized',
	'get_my_offers_amount_total'			=>	'(e)serialized',
	'get_visitors_of_chest'					=>	'(e)serialized',
	'get_shop_alternative_chest_items'		=>	'(e)serialized',
	'visit_chest'							=>	'(e)serialized',
	'unvisit_chest'							=>	'(e)serialized',
	'unvisit_all_items'						=>	'(e)serialized',
	'release_item_locks'					=>	'(e)serialized',
	'put_items_into_chest'					=>	'(e)serialized',
	'get_item_shop_price_table'				=>	'(e)serialized',
	'give_item_away'						=>	'(e)serialized'
);

$GP = $cryptObj->load_GET_POST($expected_data);

$r = new ClientServerResponse();
if (empty($GP)) {
	echo "<b>Warning</b>: No known command transmitted.<br/>\n";
	$data = (!empty($_GET)) ? $_GET : $_POST;
	echo "<br/>\nThe following transmitted commands are unknown:<br/>\n";
	foreach ($data as $command => $params) {
		if ($command === 'PHPSESSID')
			continue;
		$params = unserialize($params);
		if (is_array($params))
			echo "- $command(".implode(', ', $params).")<br/>\n";
		else
			echo "- $command($params)<br/>\n";
	}
	echo "<br/>\n";
}
else {
	foreach ($GP as $command => $params) {
		try {
			$r->add_value(TheInterface::$command($params));
		}
		catch (Exception $e) {
			error_log("File: {$e->getFile()}, Line: {$e->getLine()}, ".
				"Message: {$e->getMessage()}, Code: {$e->getCode()}");
			$r->add_exception($e);
		}
		catch (ExceptionWithErrorlog $e) {
			error_log("File: {$e->getFile()}, Line: {$e->getLine()}, ".
				"Message: {$e->getMessage()}, Code: {$e->getCode()}");
			$r->add_exception(new Exception($e->getMessage(), $e->getCode()));
		}
		catch (ExceptionWithErrormail $e) {
			error_log("File: {$e->getFile()}, Line: {$e->getLine()}, ".
				"Message: {$e->getMessage()}, Code: {$e->getCode()}");
			$r->add_exception(new Exception($e->getMessage(), $e->getCode()));
		}
	}
}
echo $r;
session_write_close(); // anti-deadlock
?>