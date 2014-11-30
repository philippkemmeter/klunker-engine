<?php

/**
 * Konstantendefinition für den EventLog
 *
 * Die Codes sind alles Bits, damit man komprimiert Filter in  die DB schreiben
 * kann. Deswegen  kann eine  Gruppe nicht mehr  als 64  Codes enthalten, da es
 * keine größeren Blöcke gibt, als welche mit 64 Bit.
 * Zudem sollte alles in 8, 16, 24, 32 oder 64er Gruppen  eingeteilt werden, da
 * das die Mysql-Blockgrößen sind.
 *
 * @author Philipp Kemmeter
 */
interface IEventLog {
	// Alle Payment-Codes müssen  zwischen MIN und MAX  liegen. MIN und MAX ist
	// _unveränderlich_, die Codes sind mit mammun_shared/classes/IEventLog.inc
	// sychron zu halten.

	/* Payment-Events: [16 Codes] */
	const TYPE_PAYMENT_MIN = 96;

	/** Klunker wurden erfolgreich aufgeladen */
	const TYPE_KLUNKER_RECEIVED = 100;	// TODO
	/** Mein Angebot wurde akzeptiert */
	const TYPE_TRADE_ACCEPTED = 101;
	/** Mein Angebot wurde abgelehnt */
	const TYPE_TRADE_DENIED = 102;
	/** Ich habe ein Geschenk erhalten */
	const TYPE_GIFT_RECEIVED = 103;
	/** Ich habe ein Item direkt verkauft */
	const TYPE_ITEM_SOLD_DIRECTLY = 104;

	const TYPE_PAYMENT_MAX = 111;
}