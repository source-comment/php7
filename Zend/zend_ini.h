/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2018 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Author: Zeev Suraski <zeev@zend.com>                                 |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_INI_H
#define ZEND_INI_H

#define ZEND_INI_USER	(1<<0)	//配置项可以用户脚本中修改
#define ZEND_INI_PERDIR	(1<<1) //配置项可以在php.ini、httpd.conf或.htaccess文件中修改
#define ZEND_INI_SYSTEM	(1<<2)	//配置项可以在php.ini 和 httpd.conf 文件中修改

#define ZEND_INI_ALL (ZEND_INI_USER|ZEND_INI_PERDIR|ZEND_INI_SYSTEM)

#define ZEND_INI_MH(name) int name(zend_ini_entry *entry, zend_string *new_value, void *mh_arg1, void *mh_arg2, void *mh_arg3, int stage)
#define ZEND_INI_DISP(name) void name(zend_ini_entry *ini_entry, int type)

//默认配置结构体
typedef struct _zend_ini_entry_def {
	const char *name;	//配置名
	ZEND_INI_MH((*on_modify));	//修改函数
	void *mh_arg1;
	void *mh_arg2;
	void *mh_arg3;
	const char *value;	//默认值
	void (*displayer)(zend_ini_entry *ini_entry, int type);	//展示配置项
	int modifiable;	//权限属性

	uint32_t name_length;	//配置名长度
	uint32_t value_length;	//值长度
} zend_ini_entry_def;

//配置存储结构体
struct _zend_ini_entry {
	zend_string *name;	//配置名称
	ZEND_INI_MH((*on_modify));	//更新函数
	void *mh_arg1;
	void *mh_arg2;
	void *mh_arg3;
	zend_string *value;	//配置值
	zend_string *orig_value;	//配置原始值
	void (*displayer)(zend_ini_entry *ini_entry, int type);	//展示函数
	int modifiable;	//权限属性，是否允许修改

	int orig_modifiable;
	int modified;	//是否修改过
	int module_number;
};

BEGIN_EXTERN_C()
ZEND_API int zend_ini_startup(void);
ZEND_API int zend_ini_shutdown(void);
ZEND_API int zend_ini_global_shutdown(void);
ZEND_API int zend_ini_deactivate(void);
ZEND_API void zend_ini_dtor(HashTable *ini_directives);

ZEND_API int zend_copy_ini_directives(void);

ZEND_API void zend_ini_sort_entries(void);

ZEND_API int zend_register_ini_entries(const zend_ini_entry_def *ini_entry, int module_number);
ZEND_API void zend_unregister_ini_entries(int module_number);
ZEND_API void zend_ini_refresh_caches(int stage);
ZEND_API int zend_alter_ini_entry(zend_string *name, zend_string *new_value, int modify_type, int stage);
ZEND_API int zend_alter_ini_entry_ex(zend_string *name, zend_string *new_value, int modify_type, int stage, int force_change);
ZEND_API int zend_alter_ini_entry_chars(zend_string *name, const char *value, size_t value_length, int modify_type, int stage);
ZEND_API int zend_alter_ini_entry_chars_ex(zend_string *name, const char *value, size_t value_length, int modify_type, int stage, int force_change);
ZEND_API int zend_restore_ini_entry(zend_string *name, int stage);
ZEND_API void display_ini_entries(zend_module_entry *module);

ZEND_API zend_long zend_ini_long(char *name, uint32_t name_length, int orig);
ZEND_API double zend_ini_double(char *name, uint32_t name_length, int orig);
ZEND_API char *zend_ini_string(char *name, uint32_t name_length, int orig);
ZEND_API char *zend_ini_string_ex(char *name, uint32_t name_length, int orig, zend_bool *exists);

ZEND_API int zend_ini_register_displayer(char *name, uint32_t name_length, void (*displayer)(zend_ini_entry *ini_entry, int type));

ZEND_API ZEND_INI_DISP(zend_ini_boolean_displayer_cb);
ZEND_API ZEND_INI_DISP(zend_ini_color_displayer_cb);
ZEND_API ZEND_INI_DISP(display_link_numbers);
END_EXTERN_C()

//配置声明的开始和结束
#define ZEND_INI_BEGIN()		static const zend_ini_entry_def ini_entries[] = {
#define ZEND_INI_END()		{ NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0} };

#define ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, arg1, arg2, arg3, displayer) \
	{ name, on_modify, arg1, arg2, arg3, default_value, displayer, modifiable, sizeof(name)-1, sizeof(default_value)-1 },

#define ZEND_INI_ENTRY3(name, default_value, modifiable, on_modify, arg1, arg2, arg3) \
	ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, arg1, arg2, arg3, NULL)

#define ZEND_INI_ENTRY2_EX(name, default_value, modifiable, on_modify, arg1, arg2, displayer) \
	ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, arg1, arg2, NULL, displayer)

#define ZEND_INI_ENTRY2(name, default_value, modifiable, on_modify, arg1, arg2) \
	ZEND_INI_ENTRY2_EX(name, default_value, modifiable, on_modify, arg1, arg2, NULL)

#define ZEND_INI_ENTRY1_EX(name, default_value, modifiable, on_modify, arg1, displayer) \
	ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, arg1, NULL, NULL, displayer)

#define ZEND_INI_ENTRY1(name, default_value, modifiable, on_modify, arg1) \
	ZEND_INI_ENTRY1_EX(name, default_value, modifiable, on_modify, arg1, NULL)

#define ZEND_INI_ENTRY_EX(name, default_value, modifiable, on_modify, displayer) \
	ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, NULL, NULL, NULL, displayer)

#define ZEND_INI_ENTRY(name, default_value, modifiable, on_modify) \
	ZEND_INI_ENTRY_EX(name, default_value, modifiable, on_modify, NULL)

#ifdef ZTS
#define STD_ZEND_INI_ENTRY(name, default_value, modifiable, on_modify, property_name, struct_type, struct_ptr) \
	ZEND_INI_ENTRY2(name, default_value, modifiable, on_modify, (void *) XtOffsetOf(struct_type, property_name), (void *) &struct_ptr##_id)
#define STD_ZEND_INI_ENTRY_EX(name, default_value, modifiable, on_modify, property_name, struct_type, struct_ptr, displayer) \
	ZEND_INI_ENTRY2_EX(name, default_value, modifiable, on_modify, (void *) XtOffsetOf(struct_type, property_name), (void *) &struct_ptr##_id, displayer)
#define STD_ZEND_INI_BOOLEAN(name, default_value, modifiable, on_modify, property_name, struct_type, struct_ptr) \
	ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, (void *) XtOffsetOf(struct_type, property_name), (void *) &struct_ptr##_id, NULL, zend_ini_boolean_displayer_cb)
#else
#define STD_ZEND_INI_ENTRY(name, default_value, modifiable, on_modify, property_name, struct_type, struct_ptr) \
	ZEND_INI_ENTRY2(name, default_value, modifiable, on_modify, (void *) XtOffsetOf(struct_type, property_name), (void *) &struct_ptr)
#define STD_ZEND_INI_ENTRY_EX(name, default_value, modifiable, on_modify, property_name, struct_type, struct_ptr, displayer) \
	ZEND_INI_ENTRY2_EX(name, default_value, modifiable, on_modify, (void *) XtOffsetOf(struct_type, property_name), (void *) &struct_ptr, displayer)
#define STD_ZEND_INI_BOOLEAN(name, default_value, modifiable, on_modify, property_name, struct_type, struct_ptr) \
	ZEND_INI_ENTRY3_EX(name, default_value, modifiable, on_modify, (void *) XtOffsetOf(struct_type, property_name), (void *) &struct_ptr, NULL, zend_ini_boolean_displayer_cb)
#endif

//获取整形，浮点型，字符串型，布尔型配置的当前值
#define INI_INT(name) zend_ini_long((name), sizeof(name)-1, 0)
#define INI_FLT(name) zend_ini_double((name), sizeof(name)-1, 0)
#define INI_STR(name) zend_ini_string_ex((name), sizeof(name)-1, 0, NULL)
#define INI_BOOL(name) ((zend_bool) INI_INT(name))

//获取整形，浮点型，字符串型，布尔型配置的初始值
#define INI_ORIG_INT(name)	zend_ini_long((name), sizeof(name)-1, 1)
#define INI_ORIG_FLT(name)	zend_ini_double((name), sizeof(name)-1, 1)
#define INI_ORIG_STR(name)	zend_ini_string((name), sizeof(name)-1, 1)
#define INI_ORIG_BOOL(name) ((zend_bool) INI_ORIG_INT(name))

#define REGISTER_INI_ENTRIES() zend_register_ini_entries(ini_entries, module_number)
#define UNREGISTER_INI_ENTRIES() zend_unregister_ini_entries(module_number)
#define DISPLAY_INI_ENTRIES() display_ini_entries(zend_module)

#define REGISTER_INI_DISPLAYER(name, displayer) zend_ini_register_displayer((name), sizeof(name)-1, displayer)
#define REGISTER_INI_BOOLEAN(name) REGISTER_INI_DISPLAYER(name, zend_ini_boolean_displayer_cb)

/* Standard message handlers */
BEGIN_EXTERN_C()
ZEND_API ZEND_INI_MH(OnUpdateBool);
ZEND_API ZEND_INI_MH(OnUpdateLong);
ZEND_API ZEND_INI_MH(OnUpdateLongGEZero);
ZEND_API ZEND_INI_MH(OnUpdateReal);
ZEND_API ZEND_INI_MH(OnUpdateString);
ZEND_API ZEND_INI_MH(OnUpdateStringUnempty);
END_EXTERN_C()

#define ZEND_INI_DISPLAY_ORIG	1	//显示初始值
#define ZEND_INI_DISPLAY_ACTIVE	2	//显示激活

#define ZEND_INI_STAGE_STARTUP		(1<<0)
#define ZEND_INI_STAGE_SHUTDOWN		(1<<1)
#define ZEND_INI_STAGE_ACTIVATE		(1<<2)
#define ZEND_INI_STAGE_DEACTIVATE	(1<<3)
#define ZEND_INI_STAGE_RUNTIME		(1<<4)
#define ZEND_INI_STAGE_HTACCESS		(1<<5)

/* INI parsing engine */
typedef void (*zend_ini_parser_cb_t)(zval *arg1, zval *arg2, zval *arg3, int callback_type, void *arg);
BEGIN_EXTERN_C()
ZEND_API int zend_parse_ini_file(zend_file_handle *fh, zend_bool unbuffered_errors, int scanner_mode, zend_ini_parser_cb_t ini_parser_cb, void *arg);
ZEND_API int zend_parse_ini_string(char *str, zend_bool unbuffered_errors, int scanner_mode, zend_ini_parser_cb_t ini_parser_cb, void *arg);
END_EXTERN_C()

/* INI entries */
#define ZEND_INI_PARSER_ENTRY     1 /* Normal entry: foo = bar */
#define ZEND_INI_PARSER_SECTION	  2 /* Section: [foobar] */
#define ZEND_INI_PARSER_POP_ENTRY 3 /* Offset entry: foo[] = bar */

typedef struct _zend_ini_parser_param {
	zend_ini_parser_cb_t ini_parser_cb;
	void *arg;
} zend_ini_parser_param;

#endif /* ZEND_INI_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
