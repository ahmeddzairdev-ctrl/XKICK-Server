SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for tb_log_connect_26_01
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_26_01`;
CREATE TABLE `tb_log_connect_26_01`  (
  `connect_seq` int NOT NULL AUTO_INCREMENT,
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0,
  PRIMARY KEY (`connect_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_connect_26_02
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_26_02`;
CREATE TABLE `tb_log_connect_26_02`  (
  `connect_seq` int NOT NULL AUTO_INCREMENT,
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0,
  PRIMARY KEY (`connect_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 139 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_connect_26_03
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_26_03`;
CREATE TABLE `tb_log_connect_26_03`  (
  `connect_seq` int NOT NULL AUTO_INCREMENT,
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0,
  PRIMARY KEY (`connect_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_connect_26_04
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_26_04`;
CREATE TABLE `tb_log_connect_26_04`  (
  `connect_seq` int NOT NULL AUTO_INCREMENT,
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0,
  PRIMARY KEY (`connect_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_connect_26_06
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_26_06`;
CREATE TABLE `tb_log_connect_26_06`  (
  `connect_seq` int NOT NULL AUTO_INCREMENT,
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0,
  PRIMARY KEY (`connect_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_connect_yy_qq
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_yy_qq`;
CREATE TABLE `tb_log_connect_yy_qq`  (
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_sale_yy_qq
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_sale_yy_qq`;
CREATE TABLE `tb_log_sale_yy_qq`  (
  `sale_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `object_seq` int NULL DEFAULT 0,
  `object_code` int NULL DEFAULT 0,
  `object_kind` int NULL DEFAULT 0,
  `buy_kind` int NULL DEFAULT 0,
  `sale_kind` int NULL DEFAULT 0,
  `sale_price` int NULL DEFAULT 0,
  `sale_amount` int NULL DEFAULT 0,
  `store_kind` int NULL DEFAULT 0,
  `sale_buydate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`sale_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_mst_playlog_26_06
-- ----------------------------
DROP TABLE IF EXISTS `tb_mst_playlog_26_06`;
CREATE TABLE `tb_mst_playlog_26_06`  (
  `playlog_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `server_code` int NOT NULL,
  `playlog_exp_in` int NULL DEFAULT 0,
  `playlog_exp_out` int NULL DEFAULT 0,
  `playlog_match_in` int NULL DEFAULT 0,
  `playlog_match_out` int NULL DEFAULT 0,
  `playlog_cash_in` int NULL DEFAULT 0,
  `playlog_cash_out` int NULL DEFAULT 0,
  `playlog_point_in` int NULL DEFAULT 0,
  `playlog_point_out` int NULL DEFAULT 0,
  `playlog_remoteip` varchar(20) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `playlog_date_in` datetime NULL DEFAULT NULL,
  `playlog_date_out` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`playlog_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 28 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
