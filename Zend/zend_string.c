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
   | Authors: Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+
*/

/* $Id: $ */

#include "zend.h"
#include "zend_globals.h"

ZEND_API zend_string *(*zend_new_interned_string)(zend_string *str);

static zend_string *zend_new_interned_string_permanent(zend_string *str);
static zend_string *zend_new_interned_string_request(zend_string *str);

/* Any strings interned in the startup phase. Common to all the threads,
   won't be free'd until process exit. If we want an ability to
   add permanent strings even after startup, it would be still
   possible on costs of locking in the thread safe builds. */
static HashTable interned_strings_permanent;

static zend_new_interned_string_func_t interned_string_request_handler = zend_new_interned_string_request;
static zend_string_copy_storage_func_t interned_string_copy_storage = NULL;

ZEND_API zend_string  *zend_empty_string = NULL;
ZEND_API zend_string  *zend_one_char_string[256];
ZEND_API zend_string **zend_known_strings = NULL;

/**
 * @description: 计算哈希值，times 33算法
 * @param char* 需要计算哈希值的字符串
 * @param size_t len 字符串长度 
 * @return zend_ulong
 */
ZEND_API zend_ulong zend_hash_func(const char *str, size_t len)
{
	return zend_inline_hash_func(str, len);
}

/**
 * @description: 销毁字符串函数
 * @param zval 要销毁的字符串
 * @return : void
 */
static void _str_dtor(zval *zv)
{
	zend_string *str = Z_STR_P(zv);
	pefree(str, GC_FLAGS(str) & IS_STR_PERSISTENT);
}

/**
 * @description: 已知字符串的数组（PHP语言本身的关键词）
 * @param void
 * @return: void
 */
static const char *known_strings[] = {
#define _ZEND_STR_DSC(id, str) str,
ZEND_KNOWN_STRINGS(_ZEND_STR_DSC)
#undef _ZEND_STR_DSC
	NULL
};

/**
 * @description: 初始化内部字符串哈希表
 * @param  HashTable* interned_strings 哈希表指针
 * @param int permanent 是否永久存储（不使用Zend内存池，直接向系统申请）
 * @return: void
 */
static void zend_init_interned_strings_ht(HashTable *interned_strings, int permanent)
{
	zend_hash_init(interned_strings, 1024, NULL, _str_dtor, permanent);	//初始化哈希表
	zend_hash_real_init(interned_strings, 0);	//初始化bucket
}

/**
 * @description: 初始化内部字符字典，并且将php已知的标识符数组写入字典
 * @param void
 * @return void
 */
ZEND_API void zend_interned_strings_init(void)
{
	char s[2];
	int i;
	zend_string *str;

	interned_string_request_handler = zend_new_interned_string_request;
	interned_string_copy_storage = NULL;

	zend_empty_string = NULL;
	zend_known_strings = NULL;

	//初始化内部字符串字典，实际初始化了一个哈希表
	zend_init_interned_strings_ht(&interned_strings_permanent, 1);

	zend_new_interned_string = zend_new_interned_string_permanent;

	/* interned empty string */
	str = zend_string_alloc(sizeof("")-1, 1);
	ZSTR_VAL(str)[0] = '\000';
	zend_empty_string = zend_new_interned_string_permanent(str);

	s[1] = 0;
	for (i = 0; i < 256; i++) {
		s[0] = i;
		zend_one_char_string[i] = zend_new_interned_string_permanent(zend_string_init(s, 1, 1));
	}

	/* known strings */
	zend_known_strings = pemalloc(sizeof(zend_string*) * ((sizeof(known_strings) / sizeof(known_strings[0]) - 1)), 1);
	for (i = 0; i < (sizeof(known_strings) / sizeof(known_strings[0])) - 1; i++) {
		str = zend_string_init(known_strings[i], strlen(known_strings[i]), 1);
		zend_known_strings[i] = zend_new_interned_string_permanent(str);
	}
}

/**
 * @description: 销毁内部字符串哈希表
 * @param void 
 * @return: 
 */
ZEND_API void zend_interned_strings_dtor(void)
{
	zend_hash_destroy(&interned_strings_permanent);

	free(zend_known_strings);
	zend_known_strings = NULL;
}

/**
 * @description: 根据字符串查找哈希表
 * @param zend_string* str 要查找的字符串
 * @param HashTable* interned_strings 待查找的内部字符串哈希表
 * @return: NULL | zval 找到返回zval，未找到返回NULL
 */
static zend_always_inline zend_string *zend_interned_string_ht_lookup(zend_string *str, HashTable *interned_strings)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;

	h = zend_string_hash_val(str); //字符串冗余hash值
	nIndex = h | interned_strings->nTableMask; //计算所在bucket的索引
	idx = HT_HASH(interned_strings, nIndex); //根据bucket的索引获取所在bucket的位置
	while (idx != HT_INVALID_IDX) {
		p = HT_HASH_TO_BUCKET(interned_strings, idx);
		if ((p->h == h) && (ZSTR_LEN(p->key) == ZSTR_LEN(str))) { //如果哈希值和长度都相等
			if (!memcmp(ZSTR_VAL(p->key), ZSTR_VAL(str), ZSTR_LEN(str))) { //内存中的数据也相等
				return p->key;
			}
		}
		idx = Z_NEXT(p->val); //下一个
	}

	//未找到
	return NULL;
}

/* This function might be not thread safe at least because it would update the
   hash val in the passed string. Be sure it is called in the appropriate context. */

/**
 * @description: 插入字符串到哈希表并设置相应的flags
 * @param zend_string* str
 * @param HashTable* interned_strings
 * @param uint32_t flags
 * @return: 
 */
static zend_always_inline zend_string *zend_add_interned_string(zend_string *str, HashTable *interned_strings, uint32_t flags)
{
	zval val;

	GC_REFCOUNT(str) = 1; //设置gc.refcount = 1
	GC_FLAGS(str) |= IS_STR_INTERNED | flags; //设置属性为内部字符串和自定义属性

	ZVAL_INTERNED_STR(&val, str); //将字符串赋值给zval

	zend_hash_add_new(interned_strings, str, &val); //将zval插入哈希表

	return str;
}

/**
 * @description:  查找内部字符串哈希表
 * @param zend_string* str 要查找的字符串
 * @return: zend_string|NULL 找到的字符串 未找到返回NULL
 */
ZEND_API zend_string *zend_interned_string_find_permanent(zend_string *str)
{
	return zend_interned_string_ht_lookup(str, &interned_strings_permanent);
}

/**
 * @description: 新增字符串到内部字符串哈希表
 * @param zend_string* str
 * @return: zend_string
 */
static zend_string *zend_new_interned_string_permanent(zend_string *str)
{
	zend_string *ret;

	//如果是内部字符串,直接返回
	if (ZSTR_IS_INTERNED(str)) {
		return str;
	}

	//查找内部哈希表并释放
	ret = zend_interned_string_ht_lookup(str, &interned_strings_permanent);
	if (ret) {
		zend_string_release(str);
		return ret;
	}

	//加入到内部字符串哈希表，并设置成永久属性
	return zend_add_interned_string(str, &interned_strings_permanent, IS_STR_PERMANENT);
}

static zend_string *zend_new_interned_string_request(zend_string *str)
{
	zend_string *ret;

	if (ZSTR_IS_INTERNED(str)) {
		return str;
	}

	/* Check for permanent strings, the table is readonly at this point. */
	ret = zend_interned_string_ht_lookup(str, &interned_strings_permanent);
	if (ret) {
		zend_string_release(str);
		return ret;
	}

	ret = zend_interned_string_ht_lookup(str, &CG(interned_strings));
	if (ret) {
		zend_string_release(str);
		return ret;
	}

	/* Create a short living interned, freed after the request. */
	ret = zend_add_interned_string(str, &CG(interned_strings), 0);

	return ret;
}

/**
 * @description: 激活编译阶段全局的内部字符串哈希表
 * @param void
 * @return: void
 */
ZEND_API void zend_interned_strings_activate(void)
{
	zend_init_interned_strings_ht(&CG(interned_strings), 0);
}

ZEND_API void zend_interned_strings_deactivate(void)
{
	zend_hash_destroy(&CG(interned_strings));
}

ZEND_API void zend_interned_strings_set_request_storage_handler(zend_new_interned_string_func_t handler)
{
	interned_string_request_handler = handler;
}

ZEND_API void zend_interned_strings_set_permanent_storage_copy_handler(zend_string_copy_storage_func_t handler)
{
	interned_string_copy_storage = handler;
}

ZEND_API void zend_interned_strings_switch_storage(void)
{
	if (interned_string_copy_storage) {
		interned_string_copy_storage();
	}
	zend_new_interned_string = interned_string_request_handler;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
