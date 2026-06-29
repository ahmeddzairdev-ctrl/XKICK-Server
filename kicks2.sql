SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for tb_game_blacklist
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_blacklist`;
CREATE TABLE `tb_game_blacklist`  (
  `black_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `member_seq` int NOT NULL,
  `target_seq` int NOT NULL,
  PRIMARY KEY (`black_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_buddy
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_buddy`;
CREATE TABLE `tb_game_buddy`  (
  `buddy_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `member_seq` int NOT NULL,
  `target_seq` int NOT NULL,
  PRIMARY KEY (`buddy_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_card
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_card`;
CREATE TABLE `tb_game_card`  (
  `card_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `member_seq` int NOT NULL,
  `card_state` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '1',
  `card_code` int NOT NULL,
  `card_equip1` int NULL DEFAULT 0,
  `card_equip2` int NULL DEFAULT 0,
  `card_equip3` tinyint NULL DEFAULT 0,
  `card_level` tinyint NULL DEFAULT 0,
  `card_rank` tinyint NULL DEFAULT 0,
  `card_position` tinyint NULL DEFAULT 0,
  `card_area` tinyint NULL DEFAULT 0,
  `card_skill` tinyint NULL DEFAULT 0,
  `card_costume` int NULL DEFAULT 0,
  `card_tired` tinyint NULL DEFAULT 0,
  `card_buydate` datetime NULL DEFAULT NULL,
  `card_deletedate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`card_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC, `card_state` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_ceremony
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_ceremony`;
CREATE TABLE `tb_game_ceremony`  (
  `ceremony_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `ceremony_state` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '1',
  `ceremony_code` int NOT NULL,
  `ceremony_equip` tinyint NULL DEFAULT 0,
  `ceremony_buydate` datetime NULL DEFAULT NULL,
  `ceremony_deletedate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`ceremony_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC, `ceremony_state` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_event
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_event`;
CREATE TABLE `tb_game_event`  (
  `event_seq` int NOT NULL AUTO_INCREMENT,
  `event_server` int NOT NULL,
  `event_type` int NULL DEFAULT 0,
  `reward_type` int NULL DEFAULT 0,
  `reward_value` int NULL DEFAULT 0,
  `event_startdate` datetime NULL DEFAULT NULL,
  `event_enddate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`event_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_faculty
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_faculty`;
CREATE TABLE `tb_game_faculty`  (
  `player_seq` int NOT NULL,
  `faculty_run` tinyint NULL DEFAULT 0,
  `faculty_dribble` tinyint NULL DEFAULT 0,
  `faculty_quick` tinyint NULL DEFAULT 0,
  `faculty_stamina` tinyint NULL DEFAULT 0,
  `faculty_elasticity` tinyint NULL DEFAULT 0,
  `faculty_bodycheck` tinyint NULL DEFAULT 0,
  `faculty_cross` tinyint NULL DEFAULT 0,
  `faculty_shortpass` tinyint NULL DEFAULT 0,
  `faculty_longpass` tinyint NULL DEFAULT 0,
  `faculty_headshoot` tinyint NULL DEFAULT 0,
  `faculty_shortshoot` tinyint NULL DEFAULT 0,
  `faculty_longshoot` tinyint NULL DEFAULT 0,
  `faculty_keeping` tinyint NULL DEFAULT 0,
  `faculty_steal` tinyint NULL DEFAULT 0,
  `faculty_tackle` tinyint NULL DEFAULT 0,
  `faculty_catch` tinyint NULL DEFAULT 0,
  `faculty_punch` tinyint NULL DEFAULT 0,
  `faculty_guard` tinyint NULL DEFAULT 0,
  `faculty_move` tinyint NULL DEFAULT 0,
  `faculty_body` tinyint NULL DEFAULT 0,
  `faculty_pass` tinyint NULL DEFAULT 0,
  `faculty_shoot` tinyint NULL DEFAULT 0,
  `faculty_defense` tinyint NULL DEFAULT 0,
  `faculty_goalkeep` tinyint NULL DEFAULT 0,
  `faculty_talent` tinyint NULL DEFAULT 0,
  PRIMARY KEY (`player_seq`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_gift
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_gift`;
CREATE TABLE `tb_game_gift`  (
  `gift_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `gifter_seq` int NOT NULL,
  `item_seq` int NOT NULL,
  `gift_msg` varchar(120) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `gift_createdate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`gift_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC) USING BTREE,
  INDEX `idx_gifter`(`gifter_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_hack
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_hack`;
CREATE TABLE `tb_game_hack`  (
  `hack_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `hack_command` int NULL DEFAULT 0,
  `hack_type` int NULL DEFAULT 0,
  `hack_date` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`hack_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_item
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_item`;
CREATE TABLE `tb_game_item`  (
  `item_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `item_state` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '1',
  `item_code` int NOT NULL,
  `item_equip` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '0',
  `item_amount` int NOT NULL DEFAULT 1,
  `item_grade` int NULL DEFAULT 0,
  `item_limit` int NULL DEFAULT 0,
  `item_price` int NULL DEFAULT 0,
  `item_level` int NULL DEFAULT 0,
  `item_class` int NULL DEFAULT 0,
  `item_option0` int NULL DEFAULT 0,
  `item_option1` int NULL DEFAULT 0,
  `item_option2` int NULL DEFAULT 0,
  `item_option3` int NULL DEFAULT 0,
  `item_option4` int NULL DEFAULT 0,
  `item_random` int NULL DEFAULT 0,
  `item_order` int NULL DEFAULT 0,
  `item_buydate` datetime NULL DEFAULT NULL,
  `item_deletedate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`item_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC, `item_state` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_notice
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_notice`;
CREATE TABLE `tb_game_notice`  (
  `notice_seq` int NOT NULL AUTO_INCREMENT,
  `notice_text` varchar(120) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `notice_date` datetime NULL DEFAULT NULL,
  `notice_enable` tinyint NOT NULL DEFAULT 1,
  PRIMARY KEY (`notice_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_player
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_player`;
CREATE TABLE `tb_game_player`  (
  `player_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_order` int NOT NULL,
  `player_state` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '1',
  `player_name` varchar(15) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL,
  `player_position` int NULL DEFAULT 0,
  `player_condition` int NULL DEFAULT 0,
  `player_alias` int NULL DEFAULT 0,
  `player_level` int NULL DEFAULT 1,
  `player_exp` int NULL DEFAULT 0,
  `player_faculty` int NULL DEFAULT 2,
  `player_skill` int NULL DEFAULT 0,
  `player_gender` int NULL DEFAULT 0,
  `player_skin` int NULL DEFAULT 0,
  `player_uniform` int NULL DEFAULT 0,
  `player_face` int NULL DEFAULT 0,
  `player_hair` int NULL DEFAULT 0,
  `player_shirts` int NULL DEFAULT 0,
  `player_pants` int NULL DEFAULT 0,
  `player_shoes` int NULL DEFAULT 0,
  `player_createdate` datetime NULL DEFAULT NULL,
  `player_deletedate` datetime NULL DEFAULT NULL,
  `player_ment` varchar(45) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '',
  `player_operation` int NOT NULL DEFAULT 0,
  `player_rand` int NOT NULL DEFAULT 0,
  `player_shopstate` int NOT NULL DEFAULT 0,
  `player_cardentry` int NOT NULL DEFAULT 0,
  `player_logindate` datetime NOT NULL DEFAULT current_timestamp(),
  `player_shutupdate` datetime NOT NULL DEFAULT current_timestamp(),
  PRIMARY KEY (`player_seq`) USING BTREE,
  INDEX `idx_member`(`member_seq` ASC) USING BTREE,
  INDEX `idx_name`(`player_name` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 2 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_quest
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_quest`;
CREATE TABLE `tb_game_quest`  (
  `quest_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `quest_code` int NOT NULL,
  `quest_count` int NULL DEFAULT 0,
  `quest_playdate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`quest_seq`) USING BTREE,
  INDEX `idx_member`(`member_seq` ASC) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_rank
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_rank`;
CREATE TABLE `tb_game_rank`  (
  `player_seq` int NOT NULL,
  `rank_code` int NOT NULL,
  `rank_num` int NULL DEFAULT 0,
  PRIMARY KEY (`player_seq`, `rank_code`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_rank_26_02
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_rank_26_02`;
CREATE TABLE `tb_game_rank_26_02`  (
  `player_seq` int NOT NULL,
  `rank_code` int NOT NULL,
  `rank_num` int NULL DEFAULT 0,
  PRIMARY KEY (`player_seq`, `rank_code`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_ranking
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_ranking`;
CREATE TABLE `tb_game_ranking`  (
  `player_seq` int NOT NULL
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_ranking_week
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_ranking_week`;
CREATE TABLE `tb_game_ranking_week`  (
  `ranking_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `ranking_date` date NOT NULL,
  `ranking_a` int NULL DEFAULT 0,
  `ranking_b` int NULL DEFAULT 0,
  PRIMARY KEY (`ranking_seq`) USING BTREE,
  UNIQUE INDEX `uk_player_date`(`player_seq` ASC, `ranking_date` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_record
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_record`;
CREATE TABLE `tb_game_record`  (
  `record_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `record_match` int NULL DEFAULT 0,
  `record_win` int NULL DEFAULT 0,
  `record_draw` int NULL DEFAULT 0,
  `record_mark` int NULL DEFAULT 0,
  `record_mvp` int NULL DEFAULT 0,
  `record_goal` int NULL DEFAULT 0,
  `record_allow` int NULL DEFAULT 0,
  `record_assist` int NULL DEFAULT 0,
  `record_cut` int NULL DEFAULT 0,
  `record_shoot` int NULL DEFAULT 0,
  `record_steal` int NULL DEFAULT 0,
  `record_tackle` int NULL DEFAULT 0,
  `record_pass` int NULL DEFAULT 0,
  `record_guard` int NULL DEFAULT 0,
  `record_tryshoot` int NULL DEFAULT 0,
  `record_trysteal` int NULL DEFAULT 0,
  `record_trytackle` int NULL DEFAULT 0,
  `record_trypass` int NULL DEFAULT 0,
  `record_tryguard` int NULL DEFAULT 0,
  PRIMARY KEY (`record_seq`) USING BTREE,
  UNIQUE INDEX `uk_player`(`player_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 27 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_record_%02d_%02d
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_record_%02d_%02d`;
CREATE TABLE `tb_game_record_%02d_%02d`  (
  `record_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `record_match` int NULL DEFAULT 0,
  `record_win` int NULL DEFAULT 0,
  `record_draw` int NULL DEFAULT 0,
  `record_mark` int NULL DEFAULT 0,
  `record_mvp` int NULL DEFAULT 0,
  `record_goal` int NULL DEFAULT 0,
  `record_allow` int NULL DEFAULT 0,
  `record_assist` int NULL DEFAULT 0,
  `record_cut` int NULL DEFAULT 0,
  `record_shoot` int NULL DEFAULT 0,
  `record_steal` int NULL DEFAULT 0,
  `record_tackle` int NULL DEFAULT 0,
  `record_pass` int NULL DEFAULT 0,
  `record_guard` int NULL DEFAULT 0,
  `record_tryshoot` int NULL DEFAULT 0,
  `record_trysteal` int NULL DEFAULT 0,
  `record_trytackle` int NULL DEFAULT 0,
  `record_trypass` int NULL DEFAULT 0,
  `record_tryguard` int NULL DEFAULT 0,
  PRIMARY KEY (`record_seq`) USING BTREE,
  UNIQUE INDEX `uk_player`(`player_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_record_26_02
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_record_26_02`;
CREATE TABLE `tb_game_record_26_02`  (
  `record_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `record_match` int NULL DEFAULT 0,
  `record_win` int NULL DEFAULT 0,
  `record_draw` int NULL DEFAULT 0,
  `record_mark` int NULL DEFAULT 0,
  `record_mvp` int NULL DEFAULT 0,
  `record_goal` int NULL DEFAULT 0,
  `record_allow` int NULL DEFAULT 0,
  `record_assist` int NULL DEFAULT 0,
  `record_cut` int NULL DEFAULT 0,
  `record_shoot` int NULL DEFAULT 0,
  `record_steal` int NULL DEFAULT 0,
  `record_tackle` int NULL DEFAULT 0,
  `record_pass` int NULL DEFAULT 0,
  `record_guard` int NULL DEFAULT 0,
  `record_tryshoot` int NULL DEFAULT 0,
  `record_trysteal` int NULL DEFAULT 0,
  `record_trytackle` int NULL DEFAULT 0,
  `record_trypass` int NULL DEFAULT 0,
  `record_tryguard` int NULL DEFAULT 0,
  PRIMARY KEY (`record_seq`) USING BTREE,
  UNIQUE INDEX `uk_player`(`player_seq` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 27 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_record_week
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_record_week`;
CREATE TABLE `tb_game_record_week`  (
  `record_seq` int NOT NULL AUTO_INCREMENT,
  `player_seq` int NOT NULL,
  `record_date` date NOT NULL,
  `record_match` int NULL DEFAULT 0,
  `record_win` int NULL DEFAULT 0,
  `record_draw` int NULL DEFAULT 0,
  `record_mark` int NULL DEFAULT 0,
  `record_mvp` int NULL DEFAULT 0,
  `record_goal` int NULL DEFAULT 0,
  `record_allow` int NULL DEFAULT 0,
  `record_assist` int NULL DEFAULT 0,
  `record_cut` int NULL DEFAULT 0,
  `record_shoot` int NULL DEFAULT 0,
  `record_steal` int NULL DEFAULT 0,
  `record_tackle` int NULL DEFAULT 0,
  `record_pass` int NULL DEFAULT 0,
  `record_guard` int NULL DEFAULT 0,
  `record_tryshoot` int NULL DEFAULT 0,
  `record_trysteal` int NULL DEFAULT 0,
  `record_trytackle` int NULL DEFAULT 0,
  `record_trypass` int NULL DEFAULT 0,
  `record_tryguard` int NULL DEFAULT 0,
  PRIMARY KEY (`record_seq`) USING BTREE,
  UNIQUE INDEX `uk_player_date`(`player_seq` ASC, `record_date` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_server
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_server`;
CREATE TABLE `tb_game_server`  (
  `server_code` int NOT NULL,
  `server_state` int NOT NULL DEFAULT 1,
  `server_channel` int NOT NULL DEFAULT 0,
  `server_name` varchar(31) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `server_match` int NULL DEFAULT 0,
  `server_job` int NULL DEFAULT 0,
  `server_free` int NULL DEFAULT 0,
  `server_slevel` int NULL DEFAULT 0,
  `server_elevel` int NULL DEFAULT 0,
  `server_max` int NULL DEFAULT 0,
  `server_current` int NULL DEFAULT 0,
  `server_exip` varchar(20) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `server_port` int NULL DEFAULT 0,
  `server_inip` varchar(20) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT '127.0.0.1',
  PRIMARY KEY (`server_code`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_setting
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_setting`;
CREATE TABLE `tb_game_setting`  (
  `player_seq` int NOT NULL,
  `setting_camera` int NULL DEFAULT 0,
  `setting_zoom` int NULL DEFAULT 0,
  `setting_target` int NULL DEFAULT 0,
  `setting_camerateam` int NULL DEFAULT 0,
  `setting_radian` int NULL DEFAULT 0,
  `setting_shadow` int NULL DEFAULT 0,
  `setting_label` int NULL DEFAULT 0,
  `setting_sound` int NULL DEFAULT 0,
  `setting_music` int NULL DEFAULT 0,
  `setting_invite` int NULL DEFAULT 0,
  `setting_whisper` int NULL DEFAULT 0,
  `setting_friend` int NULL DEFAULT 0,
  `setting_up` int NULL DEFAULT 222,          -- UP    (dedicated arrow: scan 0x48-106; Table_Input 222=UP)
  `setting_down` int NULL DEFAULT 230,        -- DOWN  (scan 0x50-106; Table_Input 230=DOWN)
  `setting_left` int NULL DEFAULT 225,        -- LEFT  (scan 0x4B-106; Table_Input 225=LEFT)
  `setting_right` int NULL DEFAULT 227,       -- RIGHT (scan 0x4D-106; Table_Input 227=RIGHT)
  `setting_leftshoot` int NULL DEFAULT 30,    -- DIK_A (0x1E)
  `setting_rightshoot` int NULL DEFAULT 31,   -- DIK_S (0x1F)
  `setting_longpass` int NULL DEFAULT 32,     -- DIK_D (0x20)
  `setting_shortpass` int NULL DEFAULT 33,    -- DIK_F (0x21)
  `setting_screen` int NULL DEFAULT 44,       -- DIK_Z (0x2C)
  `setting_tackle` int NULL DEFAULT 45,       -- DIK_X (0x2D)
  `setting_steal` int NULL DEFAULT 46,        -- DIK_C (0x2E)
  `setting_quick` int NULL DEFAULT 57,        -- DIK_SPACE    (0x39)
  `setting_quick2` int NULL DEFAULT 29,       -- DIK_LCONTROL (0x1D)
  `setting_skill1` int NULL DEFAULT 2,        -- DIK_1 (0x02)
  `setting_skill2` int NULL DEFAULT 3,        -- DIK_2 (0x03)
  `setting_skill3` int NULL DEFAULT 4,        -- DIK_3 (0x04)
  `setting_skill4` int NULL DEFAULT 5,        -- DIK_4 (0x05)
  `setting_skill5` int NULL DEFAULT 6,        -- DIK_5 (0x06)
  `setting_skill_attack1` int NULL DEFAULT 0,
  `setting_skill_attack2` int NULL DEFAULT 0,
  `setting_skill_attack3` int NULL DEFAULT 0,
  `setting_skill_attack4` int NULL DEFAULT 0,
  `setting_skill_attack5` int NULL DEFAULT 0,
  `setting_skill_defence1` int NULL DEFAULT 0,
  `setting_skill_defence2` int NULL DEFAULT 0,
  `setting_skill_defence3` int NULL DEFAULT 0,
  `setting_skill_defence4` int NULL DEFAULT 0,
  `setting_skill_defence5` int NULL DEFAULT 0,
  PRIMARY KEY (`player_seq`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_skill
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_skill`;
CREATE TABLE `tb_game_skill`  (
  `skill_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `skill_state` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '1',
  `skill_code` int NOT NULL,
  `skill_equip` tinyint NULL DEFAULT 0,
  `skill_level` tinyint NULL DEFAULT 0,
  `skill_buydate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`skill_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC, `skill_state` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_training
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_training`;
CREATE TABLE `tb_game_training`  (
  `training_seq` int NOT NULL AUTO_INCREMENT,
  `member_seq` int NOT NULL,
  `player_seq` int NOT NULL,
  `training_state` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL DEFAULT '1',
  `training_code` int NOT NULL,
  `training_type` tinyint NULL DEFAULT 0,
  `training_level` tinyint NULL DEFAULT 0,
  `training_buydate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`training_seq`) USING BTREE,
  INDEX `idx_player`(`player_seq` ASC, `training_state` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_trio
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_trio`;
CREATE TABLE `tb_game_trio`  (
  `member_seq` int NOT NULL,
  `trio_server` int NOT NULL DEFAULT 0,
  `trio_cash` int NOT NULL DEFAULT 0,
  `trio_point` int NOT NULL DEFAULT 0,
  `trio_credit` int NOT NULL DEFAULT 0,
  `trio_lastseq` int NOT NULL DEFAULT 0,
  `trio_count` int NOT NULL DEFAULT 0,
  `trio_tutorial` int NOT NULL DEFAULT 0,
  `trio_quest` int NOT NULL DEFAULT 0,
  `trio_host` int NOT NULL DEFAULT 0,
  `trio_version` varchar(50) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `trio_logindate` datetime NULL DEFAULT NULL,
  `trio_deletedate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`member_seq`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_game_weather
-- ----------------------------
DROP TABLE IF EXISTS `tb_game_weather`;
CREATE TABLE `tb_game_weather`  (
  `weather_seq` int NOT NULL AUTO_INCREMENT,
  `weather_type` int NULL DEFAULT 0,
  `weather_start` time NULL DEFAULT NULL,
  `weather_end` time NULL DEFAULT NULL,
  `weather_date` int NULL DEFAULT 0,
  PRIMARY KEY (`weather_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_connect_%02d_%02d
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_connect_%02d_%02d`;
CREATE TABLE `tb_log_connect_%02d_%02d`  (
  `connect_seq` int NOT NULL AUTO_INCREMENT,
  `connect_date` datetime NULL DEFAULT NULL,
  `connect_server` int NOT NULL,
  `connect_count` int NULL DEFAULT 0,
  `relay_count` int NULL DEFAULT 0,
  `room_count` int NULL DEFAULT 0,
  PRIMARY KEY (`connect_seq`) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_log_sale_%02d_%02d
-- ----------------------------
DROP TABLE IF EXISTS `tb_log_sale_%02d_%02d`;
CREATE TABLE `tb_log_sale_%02d_%02d`  (
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
-- Table structure for tb_mst_member
-- ----------------------------
DROP TABLE IF EXISTS `tb_mst_member`;
CREATE TABLE `tb_mst_member`  (
  `member_seq` int NOT NULL AUTO_INCREMENT,
  `member_id` varchar(50) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL,
  `member_pass` varchar(65) CHARACTER SET euckr COLLATE euckr_korean_ci NOT NULL,
  `member_state` int NOT NULL DEFAULT 1,
  `member_partner` varchar(15) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `member_speed` int NULL DEFAULT 0,
  `member_register` varchar(20) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `member_createdate` datetime NULL DEFAULT NULL,
  `member_logindate` datetime NULL DEFAULT NULL,
  `member_deletedate` datetime NULL DEFAULT NULL,
  `member_blockdate` datetime NULL DEFAULT NULL,
  `member_releasedate` datetime NULL DEFAULT NULL,
  PRIMARY KEY (`member_seq`) USING BTREE,
  UNIQUE INDEX `uk_member_id`(`member_id` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 2 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_mst_meminfo
-- ----------------------------
DROP TABLE IF EXISTS `tb_mst_meminfo`;
CREATE TABLE `tb_mst_meminfo`  (
  `meminfo_seq` int NOT NULL,
  `member_register` int NULL DEFAULT NULL,
  `meminfo_memo` varchar(255) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT '',
  PRIMARY KEY (`meminfo_seq`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_mst_playlog_%02d_%02d
-- ----------------------------
DROP TABLE IF EXISTS `tb_mst_playlog_%02d_%02d`;
CREATE TABLE `tb_mst_playlog_%02d_%02d`  (
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
) ENGINE = InnoDB AUTO_INCREMENT = 1 CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

-- ----------------------------
-- Table structure for tb_mst_start
-- ----------------------------
DROP TABLE IF EXISTS `tb_mst_start`;
CREATE TABLE `tb_mst_start`  (
  `start_yn` char(1) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT 'N',
  `start_adminid` varchar(255) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL,
  `start_adminip` varchar(255) CHARACTER SET euckr COLLATE euckr_korean_ci NULL DEFAULT NULL
) ENGINE = InnoDB CHARACTER SET = cp1256 COLLATE = cp1256_general_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;

INSERT INTO `kicks2`.`tb_game_server` (`server_code`, `server_state`, `server_channel`, `server_name`, `server_match`, `server_job`, `server_free`, `server_slevel`, `server_elevel`, `server_max`, `server_current`, `server_exip`, `server_port`, `server_inip`) VALUES (1, 0, 0, 'Certify', 0, 0, 0, 0, 0, 0, 0, '127.0.0.1', 13301, '127.0.0.1');
INSERT INTO `kicks2`.`tb_game_server` (`server_code`, `server_state`, `server_channel`, `server_name`, `server_match`, `server_job`, `server_free`, `server_slevel`, `server_elevel`, `server_max`, `server_current`, `server_exip`, `server_port`, `server_inip`) VALUES (101, 2, 9, 'Game101', 0, 0, 0, 1, 6, 1000, 0, '127.0.0.1', 14000, '127.0.0.1');
INSERT INTO `kicks2`.`tb_game_server` (`server_code`, `server_state`, `server_channel`, `server_name`, `server_match`, `server_job`, `server_free`, `server_slevel`, `server_elevel`, `server_max`, `server_current`, `server_exip`, `server_port`, `server_inip`) VALUES (102, 2, 9, 'Game102', 0, 0, 0, 7, 29, 1000, 0, '127.0.0.1', 14001, '127.0.0.1');
INSERT INTO `kicks2`.`tb_game_server` (`server_code`, `server_state`, `server_channel`, `server_name`, `server_match`, `server_job`, `server_free`, `server_slevel`, `server_elevel`, `server_max`, `server_current`, `server_exip`, `server_port`, `server_inip`) VALUES (103, 2, 9, 'Game103', 0, 0, 0, 30, 999, 1000, 0, '127.0.0.1', 14002, '127.0.0.1');
INSERT INTO `kicks2`.`tb_mst_start` (`start_yn`, `start_adminid`, `start_adminip`) VALUES ('Y', '', '');
INSERT INTO `kicks2`.`tb_mst_member` (`member_seq`, `member_id`, `member_pass`, `member_state`, `member_partner`, `member_speed`, `member_register`, `member_createdate`, `member_logindate`, `member_deletedate`, `member_blockdate`, `member_releasedate`) VALUES (1, 'test', 'test1234', 1, '', 0, '2000-01-01 00:00:00', '2024-01-01 00:00:00', '2026-06-27 12:32:28', '2000-01-01 00:00:00', '2000-01-01 00:00:00', '2000-01-01 00:00:00');
INSERT INTO `kicks2`.`tb_game_trio` (`member_seq`, `trio_server`, `trio_cash`, `trio_point`, `trio_credit`, `trio_lastseq`, `trio_count`, `trio_tutorial`, `trio_quest`, `trio_host`, `trio_version`, `trio_logindate`, `trio_deletedate`) VALUES (1, 1, 1000000, 1000000, 0, 0, 2, 0, 0, 0, '', '2026-06-27 19:56:33', '2000-01-01 00:00:00');
