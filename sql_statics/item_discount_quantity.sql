-- MySQL dump 10.11
--
-- Host: localhost    Database: payment
-- ------------------------------------------------------
-- Server version	5.0.76-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `item_discount_quantity`
--

DROP TABLE IF EXISTS `item_discount_quantity`;
SET @saved_cs_client     = @@character_set_client;
SET character_set_client = utf8;
CREATE TABLE `item_discount_quantity` (
  `type` tinyint(3) unsigned NOT NULL default '0',
  `id` smallint(5) unsigned NOT NULL default '0',
  `amount` tinyint(3) unsigned NOT NULL default '0',
  `discount` tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY  (`type`,`id`,`amount`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
SET character_set_client = @saved_cs_client;

--
-- Dumping data for table `item_discount_quantity`
--

LOCK TABLES `item_discount_quantity` WRITE;
/*!40000 ALTER TABLE `item_discount_quantity` DISABLE KEYS */;
INSERT INTO `item_discount_quantity` VALUES (1,1,1,0),(1,2,1,0),(1,3,1,0),(1,5,1,0),(1,6,1,0),(1,10,1,0),(1,101,1,0),(1,102,1,0),(1,103,1,0),(1,105,1,0),(1,106,1,0),(1,110,1,0),(1,202,1,0),(2,1,1,0),(2,2,1,0),(2,3,1,0),(2,4,1,0),(2,5,1,0),(2,8,1,0),(2,9,1,0),(5,1,1,0),(5,2,1,0),(5,3,1,0),(5,4,1,0),(5,5,1,0),(5,6,1,0),(5,8,1,0),(5,10,1,0),(5,101,1,0),(10,601,1,0),(10,2601,1,0),(10,2701,1,0),(10,2801,1,0),(10,2901,1,0),(10,3001,1,0),(10,3101,1,0),(10,3201,1,0),(10,3301,1,0),(10,3401,1,0),(10,3501,1,0),(10,3601,1,0);
/*!40000 ALTER TABLE `item_discount_quantity` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2010-11-26 11:54:27
