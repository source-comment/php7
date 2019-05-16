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
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   |          Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_HASH_H
#define ZEND_HASH_H

#include "zend.h"

#define HASH_KEY_IS_STRING 1	//哈希表key是字符串
#define HASH_KEY_IS_LONG 2	//字符串key是数值
#define HASH_KEY_NON_EXISTENT 3	//key不存在

//操作掩码
#define HASH_UPDATE 			(1<<0)	//更新
#define HASH_ADD				(1<<1)	//添加
#define HASH_UPDATE_INDIRECT	(1<<2)	//更新间接元素
#define HASH_ADD_NEW			(1<<3)	//新增
#define HASH_ADD_NEXT			(1<<4)	//新增下一个

#define HASH_FLAG_PERSISTENT       (1<<0)	//永久存储，直接向系统申请内存
#define HASH_FLAG_APPLY_PROTECTION (1<<1)	//递归保护
#define HASH_FLAG_PACKED           (1<<2)	//数值索引数组
#define HASH_FLAG_INITIALIZED      (1<<3)	//已初始化
#define HASH_FLAG_STATIC_KEYS      (1<<4) /* long and interned strings */
#define HASH_FLAG_HAS_EMPTY_IND    (1<<5)	//是否有空内部字符串
#define HASH_FLAG_ALLOW_COW_VIOLATION (1<<6)	//debug用于哈希表的断言检测

//是数值索引哈希表
#define HT_IS_PACKED(ht) \
	(((ht)->u.flags & HASH_FLAG_PACKED) != 0)

//所有元素有效
#define HT_IS_WITHOUT_HOLES(ht) \
	((ht)->nNumUsed == (ht)->nNumOfElements)

//静态key并且数值索引
#define HT_HAS_STATIC_KEYS_ONLY(ht) \
	(((ht)->u.flags & (HASH_FLAG_PACKED|HASH_FLAG_STATIC_KEYS)) != 0)

//断言检测
#if ZEND_DEBUG
# define HT_ALLOW_COW_VIOLATION(ht) (ht)->u.flags |= HASH_FLAG_ALLOW_COW_VIOLATION
#else
# define HT_ALLOW_COW_VIOLATION(ht)
#endif

//哈希表的key类型
typedef struct _zend_hash_key {
	zend_ulong h;	//数值索引或者哈希值
	zend_string *key;	//字符串索引，此值为NULL时，h=数值
} zend_hash_key;

typedef zend_bool (*merge_checker_func_t)(HashTable *target_ht, zval *source_data, zend_hash_key *hash_key, void *pParam);

BEGIN_EXTERN_C()

/* startup/shutdown */
ZEND_API void ZEND_FASTCALL _zend_hash_init(HashTable *ht, uint32_t nSize, dtor_func_t pDestructor, zend_bool persistent ZEND_FILE_LINE_DC);
ZEND_API void ZEND_FASTCALL _zend_hash_init_ex(HashTable *ht, uint32_t nSize, dtor_func_t pDestructor, zend_bool persistent, zend_bool bApplyProtection ZEND_FILE_LINE_DC);
ZEND_API void ZEND_FASTCALL zend_hash_destroy(HashTable *ht);
ZEND_API void ZEND_FASTCALL zend_hash_clean(HashTable *ht);
#define zend_hash_init(ht, nSize, pHashFunction, pDestructor, persistent)						_zend_hash_init((ht), (nSize), (pDestructor), (persistent) ZEND_FILE_LINE_CC)
#define zend_hash_init_ex(ht, nSize, pHashFunction, pDestructor, persistent, bApplyProtection)		_zend_hash_init_ex((ht), (nSize), (pDestructor), (persistent), (bApplyProtection) ZEND_FILE_LINE_CC)

ZEND_API void ZEND_FASTCALL zend_hash_real_init(HashTable *ht, zend_bool packed);
ZEND_API void ZEND_FASTCALL zend_hash_packed_to_hash(HashTable *ht);
ZEND_API void ZEND_FASTCALL zend_hash_to_packed(HashTable *ht);
ZEND_API void ZEND_FASTCALL zend_hash_extend(HashTable *ht, uint32_t nSize, zend_bool packed);

/* additions/updates/changes */
ZEND_API zval* ZEND_FASTCALL _zend_hash_add_or_update(HashTable *ht, zend_string *key, zval *pData, uint32_t flag ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_update(HashTable *ht, zend_string *key,zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_update_ind(HashTable *ht, zend_string *key,zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_add(HashTable *ht, zend_string *key,zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_add_new(HashTable *ht, zend_string *key,zval *pData ZEND_FILE_LINE_DC);

//更新key val
#define zend_hash_update(ht, key, pData) \
		_zend_hash_update(ht, key, pData ZEND_FILE_LINE_CC)
//更新key，间接val
#define zend_hash_update_ind(ht, key, pData) \
		_zend_hash_update_ind(ht, key, pData ZEND_FILE_LINE_CC)

//新增key
#define zend_hash_add(ht, key, pData) \
		_zend_hash_add(ht, key, pData ZEND_FILE_LINE_CC)
//新增key，val
#define zend_hash_add_new(ht, key, pData) \
		_zend_hash_add_new(ht, key, pData ZEND_FILE_LINE_CC)

ZEND_API zval* ZEND_FASTCALL _zend_hash_str_add_or_update(HashTable *ht, const char *key, size_t len, zval *pData, uint32_t flag ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_update(HashTable *ht, const char *key, size_t len, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_update_ind(HashTable *ht, const char *key, size_t len, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_add(HashTable *ht, const char *key, size_t len, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_add_new(HashTable *ht, const char *key, size_t len, zval *pData ZEND_FILE_LINE_DC);

#define zend_hash_str_update(ht, key, len, pData) \
		_zend_hash_str_update(ht, key, len, pData ZEND_FILE_LINE_CC)
#define zend_hash_str_update_ind(ht, key, len, pData) \
		_zend_hash_str_update_ind(ht, key, len, pData ZEND_FILE_LINE_CC)
#define zend_hash_str_add(ht, key, len, pData) \
		_zend_hash_str_add(ht, key, len, pData ZEND_FILE_LINE_CC)
#define zend_hash_str_add_new(ht, key, len, pData) \
		_zend_hash_str_add_new(ht, key, len, pData ZEND_FILE_LINE_CC)

ZEND_API zval* ZEND_FASTCALL _zend_hash_index_add_or_update(HashTable *ht, zend_ulong h, zval *pData, uint32_t flag ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_add(HashTable *ht, zend_ulong h, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_add_new(HashTable *ht, zend_ulong h, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_update(HashTable *ht, zend_ulong h, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_next_index_insert(HashTable *ht, zval *pData ZEND_FILE_LINE_DC);
ZEND_API zval* ZEND_FASTCALL _zend_hash_next_index_insert_new(HashTable *ht, zval *pData ZEND_FILE_LINE_DC);

#define zend_hash_index_add(ht, h, pData) \
		_zend_hash_index_add(ht, h, pData ZEND_FILE_LINE_CC)
#define zend_hash_index_add_new(ht, h, pData) \
		_zend_hash_index_add_new(ht, h, pData ZEND_FILE_LINE_CC)
#define zend_hash_index_update(ht, h, pData) \
		_zend_hash_index_update(ht, h, pData ZEND_FILE_LINE_CC)
#define zend_hash_next_index_insert(ht, pData) \
		_zend_hash_next_index_insert(ht, pData ZEND_FILE_LINE_CC)
#define zend_hash_next_index_insert_new(ht, pData) \
		_zend_hash_next_index_insert_new(ht, pData ZEND_FILE_LINE_CC)

ZEND_API zval* ZEND_FASTCALL zend_hash_index_add_empty_element(HashTable *ht, zend_ulong h);
ZEND_API zval* ZEND_FASTCALL zend_hash_add_empty_element(HashTable *ht, zend_string *key);
ZEND_API zval* ZEND_FASTCALL zend_hash_str_add_empty_element(HashTable *ht, const char *key, size_t len);

#define ZEND_HASH_APPLY_KEEP				0
#define ZEND_HASH_APPLY_REMOVE				1<<0
#define ZEND_HASH_APPLY_STOP				1<<1

typedef int (*apply_func_t)(zval *pDest);
typedef int (*apply_func_arg_t)(zval *pDest, void *argument);
typedef int (*apply_func_args_t)(zval *pDest, int num_args, va_list args, zend_hash_key *hash_key);

ZEND_API void ZEND_FASTCALL zend_hash_graceful_destroy(HashTable *ht);
ZEND_API void ZEND_FASTCALL zend_hash_graceful_reverse_destroy(HashTable *ht);
ZEND_API void ZEND_FASTCALL zend_hash_apply(HashTable *ht, apply_func_t apply_func);
ZEND_API void ZEND_FASTCALL zend_hash_apply_with_argument(HashTable *ht, apply_func_arg_t apply_func, void *);
ZEND_API void ZEND_FASTCALL zend_hash_apply_with_arguments(HashTable *ht, apply_func_args_t apply_func, int, ...);

/* This function should be used with special care (in other words,
 * it should usually not be used).  When used with the ZEND_HASH_APPLY_STOP
 * return value, it assumes things about the order of the elements in the hash.
 * Also, it does not provide the same kind of reentrancy protection that
 * the standard apply functions do.
 */
ZEND_API void ZEND_FASTCALL zend_hash_reverse_apply(HashTable *ht, apply_func_t apply_func);


/* Deletes */
ZEND_API int ZEND_FASTCALL zend_hash_del(HashTable *ht, zend_string *key);
ZEND_API int ZEND_FASTCALL zend_hash_del_ind(HashTable *ht, zend_string *key);
ZEND_API int ZEND_FASTCALL zend_hash_str_del(HashTable *ht, const char *key, size_t len);
ZEND_API int ZEND_FASTCALL zend_hash_str_del_ind(HashTable *ht, const char *key, size_t len);
ZEND_API int ZEND_FASTCALL zend_hash_index_del(HashTable *ht, zend_ulong h);
ZEND_API void ZEND_FASTCALL zend_hash_del_bucket(HashTable *ht, Bucket *p);

/* Data retreival */
ZEND_API zval* ZEND_FASTCALL zend_hash_find(const HashTable *ht, zend_string *key);
ZEND_API zval* ZEND_FASTCALL zend_hash_str_find(const HashTable *ht, const char *key, size_t len);
ZEND_API zval* ZEND_FASTCALL zend_hash_index_find(const HashTable *ht, zend_ulong h);
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_find(const HashTable *ht, zend_ulong h);

#define ZEND_HASH_INDEX_FIND(_ht, _h, _ret, _not_found) do { \
		if (EXPECTED((_ht)->u.flags & HASH_FLAG_PACKED)) { \
			if (EXPECTED((zend_ulong)(_h) < (zend_ulong)(_ht)->nNumUsed)) { \
				_ret = &_ht->arData[_h].val; \
				if (UNEXPECTED(Z_TYPE_P(_ret) == IS_UNDEF)) { \
					goto _not_found; \
				} \
			} else { \
				goto _not_found; \
			} \
		} else { \
			_ret = _zend_hash_index_find(_ht, _h); \
			if (UNEXPECTED(_ret == NULL)) { \
				goto _not_found; \
			} \
		} \
	} while (0)


/* Misc */
ZEND_API zend_bool ZEND_FASTCALL zend_hash_exists(const HashTable *ht, zend_string *key);
ZEND_API zend_bool ZEND_FASTCALL zend_hash_str_exists(const HashTable *ht, const char *str, size_t len);
ZEND_API zend_bool ZEND_FASTCALL zend_hash_index_exists(const HashTable *ht, zend_ulong h);

/* traversing */
#define zend_hash_has_more_elements_ex(ht, pos) \
	(zend_hash_get_current_key_type_ex(ht, pos) == HASH_KEY_NON_EXISTENT ? FAILURE : SUCCESS)
ZEND_API int   ZEND_FASTCALL zend_hash_move_forward_ex(HashTable *ht, HashPosition *pos);
ZEND_API int   ZEND_FASTCALL zend_hash_move_backwards_ex(HashTable *ht, HashPosition *pos);
ZEND_API int   ZEND_FASTCALL zend_hash_get_current_key_ex(const HashTable *ht, zend_string **str_index, zend_ulong *num_index, HashPosition *pos);
ZEND_API void  ZEND_FASTCALL zend_hash_get_current_key_zval_ex(const HashTable *ht, zval *key, HashPosition *pos);
ZEND_API int   ZEND_FASTCALL zend_hash_get_current_key_type_ex(HashTable *ht, HashPosition *pos);
ZEND_API zval* ZEND_FASTCALL zend_hash_get_current_data_ex(HashTable *ht, HashPosition *pos);
ZEND_API void  ZEND_FASTCALL zend_hash_internal_pointer_reset_ex(HashTable *ht, HashPosition *pos);
ZEND_API void  ZEND_FASTCALL zend_hash_internal_pointer_end_ex(HashTable *ht, HashPosition *pos);

#define zend_hash_has_more_elements(ht) \
	zend_hash_has_more_elements_ex(ht, &(ht)->nInternalPointer)
#define zend_hash_move_forward(ht) \
	zend_hash_move_forward_ex(ht, &(ht)->nInternalPointer)
#define zend_hash_move_backwards(ht) \
	zend_hash_move_backwards_ex(ht, &(ht)->nInternalPointer)
#define zend_hash_get_current_key(ht, str_index, num_index) \
	zend_hash_get_current_key_ex(ht, str_index, num_index, &(ht)->nInternalPointer)
#define zend_hash_get_current_key_zval(ht, key) \
	zend_hash_get_current_key_zval_ex(ht, key, &(ht)->nInternalPointer)
#define zend_hash_get_current_key_type(ht) \
	zend_hash_get_current_key_type_ex(ht, &(ht)->nInternalPointer)
#define zend_hash_get_current_data(ht) \
	zend_hash_get_current_data_ex(ht, &(ht)->nInternalPointer)
#define zend_hash_internal_pointer_reset(ht) \
	zend_hash_internal_pointer_reset_ex(ht, &(ht)->nInternalPointer)
#define zend_hash_internal_pointer_end(ht) \
	zend_hash_internal_pointer_end_ex(ht, &(ht)->nInternalPointer)

/* Copying, merging and sorting */
ZEND_API void  ZEND_FASTCALL zend_hash_copy(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor);
ZEND_API void  ZEND_FASTCALL _zend_hash_merge(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, zend_bool overwrite ZEND_FILE_LINE_DC);
ZEND_API void  ZEND_FASTCALL zend_hash_merge_ex(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, merge_checker_func_t pMergeSource, void *pParam);
ZEND_API void  zend_hash_bucket_swap(Bucket *p, Bucket *q);
ZEND_API void  zend_hash_bucket_renum_swap(Bucket *p, Bucket *q);
ZEND_API void  zend_hash_bucket_packed_swap(Bucket *p, Bucket *q);
ZEND_API int   zend_hash_compare(HashTable *ht1, HashTable *ht2, compare_func_t compar, zend_bool ordered);
ZEND_API int   ZEND_FASTCALL zend_hash_sort_ex(HashTable *ht, sort_func_t sort_func, compare_func_t compare_func, zend_bool renumber);
ZEND_API zval* ZEND_FASTCALL zend_hash_minmax(const HashTable *ht, compare_func_t compar, uint32_t flag);

#define zend_hash_merge(target, source, pCopyConstructor, overwrite)					\
	_zend_hash_merge(target, source, pCopyConstructor, overwrite ZEND_FILE_LINE_CC)

#define zend_hash_sort(ht, compare_func, renumber) \
	zend_hash_sort_ex(ht, zend_sort, compare_func, renumber)

#define zend_hash_num_elements(ht) \
	(ht)->nNumOfElements

#define zend_hash_next_free_element(ht) \
	(ht)->nNextFreeElement

ZEND_API int ZEND_FASTCALL zend_hash_rehash(HashTable *ht);

ZEND_API uint32_t zend_array_count(HashTable *ht);
ZEND_API HashTable* ZEND_FASTCALL zend_array_dup(HashTable *source);
ZEND_API void ZEND_FASTCALL zend_array_destroy(HashTable *ht);
ZEND_API void ZEND_FASTCALL zend_symtable_clean(HashTable *ht);
ZEND_API HashTable* ZEND_FASTCALL zend_symtable_to_proptable(HashTable *ht);
ZEND_API HashTable* ZEND_FASTCALL zend_proptable_to_symtable(HashTable *ht, zend_bool always_duplicate);

ZEND_API int ZEND_FASTCALL _zend_handle_numeric_str_ex(const char *key, size_t length, zend_ulong *idx);

ZEND_API uint32_t     ZEND_FASTCALL zend_hash_iterator_add(HashTable *ht, HashPosition pos);
ZEND_API HashPosition ZEND_FASTCALL zend_hash_iterator_pos(uint32_t idx, HashTable *ht);
ZEND_API HashPosition ZEND_FASTCALL zend_hash_iterator_pos_ex(uint32_t idx, zval *array);
ZEND_API void         ZEND_FASTCALL zend_hash_iterator_del(uint32_t idx);
ZEND_API HashPosition ZEND_FASTCALL zend_hash_iterators_lower_pos(HashTable *ht, HashPosition start);
ZEND_API void         ZEND_FASTCALL _zend_hash_iterators_update(HashTable *ht, HashPosition from, HashPosition to);

//更新迭代器的位置，from -> to
static zend_always_inline void zend_hash_iterators_update(HashTable *ht, HashPosition from, HashPosition to)
{
	if (UNEXPECTED(ht->u.v.nIteratorsCount)) {
		_zend_hash_iterators_update(ht, from, to);
	}
}


END_EXTERN_C()
//初始化字符表
#define ZEND_INIT_SYMTABLE(ht)								\
	ZEND_INIT_SYMTABLE_EX(ht, 8, 0)

//初始化哈希表
#define ZEND_INIT_SYMTABLE_EX(ht, n, persistent)			\
	zend_hash_init(ht, n, NULL, ZVAL_PTR_DTOR, persistent)

/**
 * @description: 将字符串转成数值，并赋值给idx
 * @param char* key 待操作字符串
 * @param size_t length 字符串长度
 * @param zend_ulong* idx 待复制idx
 * @return: int
 */
static zend_always_inline int _zend_handle_numeric_str(const char *key, size_t length, zend_ulong *idx)
{
	const char *tmp = key;

	if (*tmp > '9') {
		return 0;
	} else if (*tmp < '0') {
		if (*tmp != '-') {
			return 0;
		}
		tmp++;
		if (*tmp > '9' || *tmp < '0') {
			return 0;
		}
	}
	return _zend_handle_numeric_str_ex(key, length, idx);
}

#define ZEND_HANDLE_NUMERIC_STR(key, length, idx) \
	_zend_handle_numeric_str(key, length, &idx)

#define ZEND_HANDLE_NUMERIC(key, idx) \
	ZEND_HANDLE_NUMERIC_STR(ZSTR_VAL(key), ZSTR_LEN(key), idx)


/**
 * @description: 查找key
 * @param HashTable* ht 待查找哈希表
 * @param zend_string* key
 * @return: zal
 */
static zend_always_inline zval *zend_hash_find_ind(const HashTable *ht, zend_string *key)
{
	zval *zv;

	zv = zend_hash_find(ht, key);
	return (zv && Z_TYPE_P(zv) == IS_INDIRECT) ?
		((Z_TYPE_P(Z_INDIRECT_P(zv)) != IS_UNDEF) ? Z_INDIRECT_P(zv) : NULL) : zv;
}

/**
 * @description: 判断key是否存在
 * @param HashTable* ht 待查找哈希表
 * @param end_string* key
 * @return: int
 */
static zend_always_inline int zend_hash_exists_ind(const HashTable *ht, zend_string *key)
{
	zval *zv;

	zv = zend_hash_find(ht, key);
	return zv && (Z_TYPE_P(zv) != IS_INDIRECT ||
			Z_TYPE_P(Z_INDIRECT_P(zv)) != IS_UNDEF);
}

/**
 * @description: 查找key
 * @param HashTable* ht 待查找哈希表
 * @param char* str 查找字符串
 * @param size_t len 字符串长度
 * @return: zal
 */
static zend_always_inline zval *zend_hash_str_find_ind(const HashTable *ht, const char *str, size_t len)
{
	zval *zv;

	zv = zend_hash_str_find(ht, str, len);
	return (zv && Z_TYPE_P(zv) == IS_INDIRECT) ?
		((Z_TYPE_P(Z_INDIRECT_P(zv)) != IS_UNDEF) ? Z_INDIRECT_P(zv) : NULL) : zv;
}

/**
 * @description: 判断字符串是否存在
 * @param HashTable* ht 待查找哈希表
 * @param char* str 查找字符串
 * @param size_t len 字符串长度
 * @return: int
 */
static zend_always_inline int zend_hash_str_exists_ind(const HashTable *ht, const char *str, size_t len)
{
	zval *zv;

	zv = zend_hash_str_find(ht, str, len);
	return zv && (Z_TYPE_P(zv) != IS_INDIRECT ||
			Z_TYPE_P(Z_INDIRECT_P(zv)) != IS_UNDEF);
}

/**
 * @description: 添加到哈希表
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 插入key
 * @param zval* pData 插入元素
 * @return: zval
 */
static zend_always_inline zval *zend_symtable_add_new(HashTable *ht, zend_string *key, zval *pData)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_add_new(ht, idx, pData);
	} else {
		return zend_hash_add_new(ht, key, pData);
	}
}

/**
 * @description: 更新哈希表元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 插入key
 * @param zval* pData 插入元素
 * @return: zval
 */
static zend_always_inline zval *zend_symtable_update(HashTable *ht, zend_string *key, zval *pData)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_update(ht, idx, pData);
	} else {
		return zend_hash_update(ht, key, pData);
	}
}

/**
 * @description: 更新哈希表的间接元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 插入key
 * @param zval* pData 插入元素
 * @return: zval
 */
static zend_always_inline zval *zend_symtable_update_ind(HashTable *ht, zend_string *key, zval *pData)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_update(ht, idx, pData);
	} else {
		return zend_hash_update_ind(ht, key, pData);
	}
}

/**
 * @description: 删除哈希表key
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* 待删除key
 * @return: zval
 */
static zend_always_inline int zend_symtable_del(HashTable *ht, zend_string *key)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_del(ht, idx);
	} else {
		return zend_hash_del(ht, key);
	}
}

/**
 * @description: 删除哈希表间接元素key
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* 待删除key
 * @return: zval
 */
static zend_always_inline int zend_symtable_del_ind(HashTable *ht, zend_string *key)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_del(ht, idx);
	} else {
		return zend_hash_del_ind(ht, key);
	}
}

/**
 * @description: 查找key
 * @param HashTable* ht 待查找哈希表
 * @param zend_string* key 查找字符串
 * @return: zal
 */
static zend_always_inline zval *zend_symtable_find(const HashTable *ht, zend_string *key)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_find(ht, idx);
	} else {
		return zend_hash_find(ht, key);
	}
}

/**
 * @description: 查找数字key
 * @param HashTable* ht 待查找哈希表
 * @param zend_string* key 查找key
 * @return: zal
 */
static zend_always_inline zval *zend_symtable_find_ind(const HashTable *ht, zend_string *key)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_find(ht, idx);
	} else {
		return zend_hash_find_ind(ht, key);
	}
}

/**
 * @description: 判断数字key是否存在
 * @param HashTable* ht 待查找哈希表
 * @param zend_string* key 查找key
 * @return: zal
 */
static zend_always_inline int zend_symtable_exists(HashTable *ht, zend_string *key)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_exists(ht, idx);
	} else {
		return zend_hash_exists(ht, key);
	}
}

/**
 * @description: 判断数字间接元素key是否存在
 * @param HashTable* ht 待查找哈希表
 * @param zend_string* key 查找key
 * @return: zal
 */
static zend_always_inline int zend_symtable_exists_ind(HashTable *ht, zend_string *key)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC(key, idx)) {
		return zend_hash_index_exists(ht, idx);
	} else {
		return zend_hash_exists_ind(ht, key);
	}
}

/**
 * @description: 更新哈希表
 * @param HashTable* ht 待操作哈希表
 * @param char* str 查找字符串
 * @param size_t len 字符串长度
 * @param zval* pData
 * @return: zal
 */
static zend_always_inline zval *zend_symtable_str_update(HashTable *ht, const char *str, size_t len, zval *pData)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_update(ht, idx, pData);
	} else {
		return zend_hash_str_update(ht, str, len, pData);
	}
}

/**
 * @description: 更新哈希表间接元素
 * @param HashTable* ht 待操作哈希表
 * @param char* str 查找字符串
 * @param size_t len 字符串长度
 * @param zval* pData
 * @return: zal
 */
static zend_always_inline zval *zend_symtable_str_update_ind(HashTable *ht, const char *str, size_t len, zval *pData)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_update(ht, idx, pData);
	} else {
		return zend_hash_str_update_ind(ht, str, len, pData);
	}
}

/**
 * @description: 从哈希表删除元素
 * @param HashTable* ht 待查找哈希表
 * @param char* str 删除key
 * @param size_t len key长度
 * @return: zal
 */
static zend_always_inline int zend_symtable_str_del(HashTable *ht, const char *str, size_t len)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_del(ht, idx);
	} else {
		return zend_hash_str_del(ht, str, len);
	}
}

/**
 * @description: 从哈希表删除间接元素
 * @param HashTable* ht 待查找哈希表
 * @param char* str 删除key
 * @param size_t len key长度
 * @return: zal
 */
static zend_always_inline int zend_symtable_str_del_ind(HashTable *ht, const char *str, size_t len)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_del(ht, idx);
	} else {
		return zend_hash_str_del_ind(ht, str, len);
	}
}

/**
 * @description: 从哈希表中查找key
 * @param HashTable* ht 待查找哈希表
 * @param char* str 字符串key
 * @param size_t len key长度
 * @return: zal
 */
static zend_always_inline zval *zend_symtable_str_find(HashTable *ht, const char *str, size_t len)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_find(ht, idx);
	} else {
		return zend_hash_str_find(ht, str, len);
	}
}

/**
 * @description: 从哈希表中查找key
 * @param HashTable* ht 待查找哈希表
 * @param char* str 字符串key
 * @param size_t len key长度
 * @return: zal
 */
static zend_always_inline int zend_symtable_str_exists(HashTable *ht, const char *str, size_t len)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_exists(ht, idx);
	} else {
		return zend_hash_str_exists(ht, str, len);
	}
}

/**
 * @description: 添加指针元素到哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_string* key 插入key
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_add_ptr(HashTable *ht, zend_string *key, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_add(ht, key, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 添加指针元素到哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_string* key 插入key
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_add_new_ptr(HashTable *ht, zend_string *key, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_add_new(ht, key, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 添加指针元素到哈希表
 * @param HashTable* ht待操作哈希表
 * @param char* str 插入key
 * @param size_t len key长度
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_str_add_ptr(HashTable *ht, const char *str, size_t len, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_str_add(ht, str, len, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 添加指针元素到哈希表
 * @param HashTable* ht待操作哈希表
 * @param char* str 插入key
 * @param size_t len key长度
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_str_add_new_ptr(HashTable *ht, const char *str, size_t len, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_str_add_new(ht, str, len, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 更新哈希表指针元素
 * @param HashTable* ht待操作哈希表
 * @param zend_string* key 元素key
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_update_ptr(HashTable *ht, zend_string *key, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_update(ht, key, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 更新哈希表指针元素
 * @param HashTable* ht待操作哈希表
 * @param zend_string* key 元素key
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_str_update_ptr(HashTable *ht, const char *str, size_t len, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_str_update(ht, str, len, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 添加元素到哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_string* key 元素key
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_add_mem(HashTable *ht, zend_string *key, void *pData, size_t size)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, NULL);
	if ((zv = zend_hash_add(ht, key, &tmp))) {
		Z_PTR_P(zv) = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
		memcpy(Z_PTR_P(zv), pData, size);
		return Z_PTR_P(zv);
	}
	return NULL;
}

/**
 * @description: 添加key对应一个空元素到哈希表并复制一个新元素返回
 * @param HashTable* ht待操作哈希表
 * @param char* str 字符串key
 * @param size_t len 字符串长度
 * @param void* pData 元素指针
 * @param size_t size 元素大小
 * @return: zval*|NULL
 */
static zend_always_inline void *zend_hash_str_add_mem(HashTable *ht, const char *str, size_t len, void *pData, size_t size)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, NULL);
	if ((zv = zend_hash_str_add(ht, str, len, &tmp))) {
		Z_PTR_P(zv) = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
		memcpy(Z_PTR_P(zv), pData, size);
		return Z_PTR_P(zv);
	}
	return NULL;
}

/**
 * @description: 更新哈希表为pData的副本
 * @param HashTable* ht待操作哈希表
 * param zend_string* key
 * @param void* pData 元素指针
 * @param size_t size 元素大小
 * @return: 
 */
static zend_always_inline void *zend_hash_update_mem(HashTable *ht, zend_string *key, void *pData, size_t size)
{
	void *p;

	p = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
	memcpy(p, pData, size);
	return zend_hash_update_ptr(ht, key, p);
}

/**
 * @description: 更新哈希表为pData的副本
 * @param HashTable* ht待操作哈希表
 * @param char* str 字符串key
 * @param size_t len 字符串长度
 * @param void* pData 元素指针
 * @param size_t size 元素大小
 * @return: 
 */
static zend_always_inline void *zend_hash_str_update_mem(HashTable *ht, const char *str, size_t len, void *pData, size_t size)
{
	void *p;

	p = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
	memcpy(p, pData, size);
	return zend_hash_str_update_ptr(ht, str, len, p);
}

/**
 * @description: 添加指针元素到数值索引哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 数值
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_index_add_ptr(HashTable *ht, zend_ulong h, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_index_add(ht, h, &tmp);
	return zv ? Z_PTR_P(zv) : NULL;
}

/**
 * @description: 添加指针元素到数值索引哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 数值
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_index_add_new_ptr(HashTable *ht, zend_ulong h, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_index_add_new(ht, h, &tmp);
	return zv ? Z_PTR_P(zv) : NULL;
}

/**
 * @description: 更新指针元素到数值索引哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 数值
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_index_update_ptr(HashTable *ht, zend_ulong h, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_index_update(ht, h, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 添加空指针到数值索引哈希表，并返pData副本
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 数值
 * @param void* pData 元素指针
 * @param size_t size 元素大小
 * @return: void
 */
static zend_always_inline void *zend_hash_index_add_mem(HashTable *ht, zend_ulong h, void *pData, size_t size)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, NULL);
	if ((zv = zend_hash_index_add(ht, h, &tmp))) {
		Z_PTR_P(zv) = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
		memcpy(Z_PTR_P(zv), pData, size);
		return Z_PTR_P(zv);
	}
	return NULL;
}

/**
 * @description: 插入指针元素到数值索引哈希表
 * @param HashTable* ht待操作哈希表
 * @param void* pData 元素指针
 * @return: void
 */
static zend_always_inline void *zend_hash_next_index_insert_ptr(HashTable *ht, void *pData)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, pData);
	zv = zend_hash_next_index_insert(ht, &tmp);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 插入指针元素的副本到数值索引哈希表
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 索引
 * @param void* pData 元素指针
 * @param size_t size 元素大小
 * @return: void
 */
static zend_always_inline void *zend_hash_index_update_mem(HashTable *ht, zend_ulong h, void *pData, size_t size)
{
	void *p;

	p = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
	memcpy(p, pData, size);
	return zend_hash_index_update_ptr(ht, h, p);
}

/**
 * @description: 插入空数据到哈希表，并返回指定数据副本
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 索引
 * @param void* pData 元素指针
 * @param size_t size 元素大小
 * @return: void
 */
static zend_always_inline void *zend_hash_next_index_insert_mem(HashTable *ht, void *pData, size_t size)
{
	zval tmp, *zv;

	ZVAL_PTR(&tmp, NULL);
	if ((zv = zend_hash_next_index_insert(ht, &tmp))) {
		Z_PTR_P(zv) = pemalloc(size, ht->u.flags & HASH_FLAG_PERSISTENT);
		memcpy(Z_PTR_P(zv), pData, size);
		return Z_PTR_P(zv);
	}
	return NULL;
}

/**
 * @description: 查找指针元素
 * @param HashTable* ht待操作哈希表
 * @param zend_string* key 查找key
 * @return: void|NULL
 */
static zend_always_inline void *zend_hash_find_ptr(const HashTable *ht, zend_string *key)
{
	zval *zv;

	zv = zend_hash_find(ht, key);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 查找指针元素
 * @param HashTable* ht待操作哈希表
 * @param char* str 查找key
 * @param size_t len key长度
 * @return: void|NULL
 */
static zend_always_inline void *zend_hash_str_find_ptr(const HashTable *ht, const char *str, size_t len)
{
	zval *zv;

	zv = zend_hash_str_find(ht, str, len);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 通过索引查找指针元素
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 索引
 * @return: void|NULL
 */
static zend_always_inline void *zend_hash_index_find_ptr(const HashTable *ht, zend_ulong h)
{
	zval *zv;

	zv = zend_hash_index_find(ht, h);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

/**
 * @description: 通过索引查找指针元素并返回指针目标
 * @param HashTable* ht待操作哈希表
 * @param zend_ulong h 索引
 * @return: zval
 */
static zend_always_inline zval *zend_hash_index_find_deref(HashTable *ht, zend_ulong h)
{
	zval *zv = zend_hash_index_find(ht, h);
	if (zv) {
		ZVAL_DEREF(zv);
	}
	return zv;
}

/**
 * @description: 通过字符串查找指针元素并返回指针目标
 * @param HashTable* ht待操作哈希表
 * @param zend_string* str 字符串
 * @return: zval
 */
static zend_always_inline zval *zend_hash_find_deref(HashTable *ht, zend_string *str)
{
	zval *zv = zend_hash_find(ht, str);
	if (zv) {
		ZVAL_DEREF(zv);
	}
	return zv;
}

/**
 * @description: 通过字符串查找指针元素并返回指针目标
 * @param HashTable* ht待操作哈希表
 * @param char* str key
 * @param size_t len 长度
 * @return: zval
 */
static zend_always_inline zval *zend_hash_str_find_deref(HashTable *ht, const char *str, size_t len)
{
	zval *zv = zend_hash_str_find(ht, str, len);
	if (zv) {
		ZVAL_DEREF(zv);
	}
	return zv;
}

/**
 * @description: 通过字符串查找指针元素并返回指针目标（自动选择查找方式）
 * @param HashTable* ht待操作哈希表
 * @param char* str key
 * @param size_t len 长度
 * @return: zval
 */
static zend_always_inline void *zend_symtable_str_find_ptr(HashTable *ht, const char *str, size_t len)
{
	zend_ulong idx;

	if (ZEND_HANDLE_NUMERIC_STR(str, len, idx)) {
		return zend_hash_index_find_ptr(ht, idx);
	} else {
		return zend_hash_str_find_ptr(ht, str, len);
	}
}

/**
 * @description: 根据索引获取哈希表的指针元素数据
 * @param HashTable* ht 代操作哈希表
 * @param HashPosition* pos
 * @return: zval*
 */
static zend_always_inline void *zend_hash_get_current_data_ptr_ex(HashTable *ht, HashPosition *pos)
{
	zval *zv;

	zv = zend_hash_get_current_data_ex(ht, pos);
	if (zv) {
		ZEND_ASSUME(Z_PTR_P(zv));
		return Z_PTR_P(zv);
	} else {
		return NULL;
	}
}

//查找指针元素数据
#define zend_hash_get_current_data_ptr(ht) \
	zend_hash_get_current_data_ptr_ex(ht, &(ht)->nInternalPointer)

//遍历哈希表开始
#define ZEND_HASH_FOREACH(_ht, indirect) do { \
		HashTable *__ht = (_ht); \
		Bucket *_p = __ht->arData; \
		Bucket *_end = _p + __ht->nNumUsed; \
		for (; _p != _end; _p++) { \
			zval *_z = &_p->val; \
			if (indirect && Z_TYPE_P(_z) == IS_INDIRECT) { \
				_z = Z_INDIRECT_P(_z); \
			} \
			if (UNEXPECTED(Z_TYPE_P(_z) == IS_UNDEF)) continue;

//倒序遍历哈希表开始
#define ZEND_HASH_REVERSE_FOREACH(_ht, indirect) do { \
		HashTable *__ht = (_ht); \
		uint32_t _idx; \
		for (_idx = __ht->nNumUsed; _idx > 0; _idx--) { \
			Bucket *_p = __ht->arData + (_idx - 1); \
			zval *_z = &_p->val; \
			if (indirect && Z_TYPE_P(_z) == IS_INDIRECT) { \
				_z = Z_INDIRECT_P(_z); \
			} \
			if (UNEXPECTED(Z_TYPE_P(_z) == IS_UNDEF)) continue;

//遍历哈希表结束
#define ZEND_HASH_FOREACH_END() \
		} \
	} while (0)

#define ZEND_HASH_FOREACH_END_DEL() \
			__ht->nNumOfElements--; \
			do { \
				uint32_t j = HT_IDX_TO_HASH(_idx - 1); \
				uint32_t nIndex = _p->h | __ht->nTableMask; \
				uint32_t i = HT_HASH(__ht, nIndex); \
				if (j != i) { \
					Bucket *prev = HT_HASH_TO_BUCKET(__ht, i); \
					while (Z_NEXT(prev->val) != j) { \
						i = Z_NEXT(prev->val); \
						prev = HT_HASH_TO_BUCKET(__ht, i); \
					} \
					Z_NEXT(prev->val) = Z_NEXT(_p->val); \
				} else { \
					HT_HASH(__ht, nIndex) = Z_NEXT(_p->val); \
				} \
			} while (0); \
		} \
		__ht->nNumUsed = _idx; \
	} while (0)

#define ZEND_HASH_FOREACH_BUCKET(ht, _bucket) \
	ZEND_HASH_FOREACH(ht, 0); \
	_bucket = _p;

#define ZEND_HASH_FOREACH_VAL(ht, _val) \
	ZEND_HASH_FOREACH(ht, 0); \
	_val = _z;

#define ZEND_HASH_FOREACH_VAL_IND(ht, _val) \
	ZEND_HASH_FOREACH(ht, 1); \
	_val = _z;

#define ZEND_HASH_FOREACH_PTR(ht, _ptr) \
	ZEND_HASH_FOREACH(ht, 0); \
	_ptr = Z_PTR_P(_z);

#define ZEND_HASH_FOREACH_NUM_KEY(ht, _h) \
	ZEND_HASH_FOREACH(ht, 0); \
	_h = _p->h;

#define ZEND_HASH_FOREACH_STR_KEY(ht, _key) \
	ZEND_HASH_FOREACH(ht, 0); \
	_key = _p->key;

#define ZEND_HASH_FOREACH_KEY(ht, _h, _key) \
	ZEND_HASH_FOREACH(ht, 0); \
	_h = _p->h; \
	_key = _p->key;

#define ZEND_HASH_FOREACH_NUM_KEY_VAL(ht, _h, _val) \
	ZEND_HASH_FOREACH(ht, 0); \
	_h = _p->h; \
	_val = _z;

#define ZEND_HASH_FOREACH_STR_KEY_VAL(ht, _key, _val) \
	ZEND_HASH_FOREACH(ht, 0); \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_FOREACH_KEY_VAL(ht, _h, _key, _val) \
	ZEND_HASH_FOREACH(ht, 0); \
	_h = _p->h; \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_FOREACH_STR_KEY_VAL_IND(ht, _key, _val) \
	ZEND_HASH_FOREACH(ht, 1); \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_FOREACH_KEY_VAL_IND(ht, _h, _key, _val) \
	ZEND_HASH_FOREACH(ht, 1); \
	_h = _p->h; \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_FOREACH_NUM_KEY_PTR(ht, _h, _ptr) \
	ZEND_HASH_FOREACH(ht, 0); \
	_h = _p->h; \
	_ptr = Z_PTR_P(_z);

#define ZEND_HASH_FOREACH_STR_KEY_PTR(ht, _key, _ptr) \
	ZEND_HASH_FOREACH(ht, 0); \
	_key = _p->key; \
	_ptr = Z_PTR_P(_z);

#define ZEND_HASH_FOREACH_KEY_PTR(ht, _h, _key, _ptr) \
	ZEND_HASH_FOREACH(ht, 0); \
	_h = _p->h; \
	_key = _p->key; \
	_ptr = Z_PTR_P(_z);

#define ZEND_HASH_REVERSE_FOREACH_BUCKET(ht, _bucket) \
	ZEND_HASH_REVERSE_FOREACH(ht, 0); \
	_bucket = _p;

#define ZEND_HASH_REVERSE_FOREACH_VAL(ht, _val) \
	ZEND_HASH_REVERSE_FOREACH(ht, 0); \
	_val = _z;

#define ZEND_HASH_REVERSE_FOREACH_PTR(ht, _ptr) \
	ZEND_HASH_REVERSE_FOREACH(ht, 0); \
	_ptr = Z_PTR_P(_z);

#define ZEND_HASH_REVERSE_FOREACH_VAL_IND(ht, _val) \
	ZEND_HASH_REVERSE_FOREACH(ht, 1); \
	_val = _z;

#define ZEND_HASH_REVERSE_FOREACH_STR_KEY_VAL(ht, _key, _val) \
	ZEND_HASH_REVERSE_FOREACH(ht, 0); \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_REVERSE_FOREACH_KEY_VAL(ht, _h, _key, _val) \
	ZEND_HASH_REVERSE_FOREACH(ht, 0); \
	_h = _p->h; \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_REVERSE_FOREACH_KEY_VAL_IND(ht, _h, _key, _val) \
	ZEND_HASH_REVERSE_FOREACH(ht, 1); \
	_h = _p->h; \
	_key = _p->key; \
	_val = _z;

#define ZEND_HASH_APPLY_PROTECTION(ht) \
	((ht)->u.flags & HASH_FLAG_APPLY_PROTECTION)

#define ZEND_HASH_APPLY_SHIFT         8
#define ZEND_HASH_APPLY_COUNT_MASK    0xff00
#define ZEND_HASH_GET_APPLY_COUNT(ht) (((ht)->u.flags & ZEND_HASH_APPLY_COUNT_MASK) >> ZEND_HASH_APPLY_SHIFT)
#define ZEND_HASH_INC_APPLY_COUNT(ht) ((ht)->u.flags += (1 << ZEND_HASH_APPLY_SHIFT))
#define ZEND_HASH_DEC_APPLY_COUNT(ht) ((ht)->u.flags -= (1 << ZEND_HASH_APPLY_SHIFT))


/* The following macros are useful to insert a sequence of new elements
 * of packed array. They may be use insted of series of
 * zend_hash_next_index_insert_new()
 * (HashTable must have enough free buckets).
 */
#define ZEND_HASH_FILL_PACKED(ht) do { \
		HashTable *__fill_ht = (ht); \
		Bucket *__fill_bkt = __fill_ht->arData + __fill_ht->nNumUsed; \
		uint32_t __fill_idx = __fill_ht->nNumUsed; \
		ZEND_ASSERT(__fill_ht->u.flags & HASH_FLAG_PACKED);

#define ZEND_HASH_FILL_ADD(_val) do { \
		ZVAL_COPY_VALUE(&__fill_bkt->val, _val); \
		__fill_bkt->h = (__fill_idx); \
		__fill_bkt->key = NULL; \
		__fill_bkt++; \
		__fill_idx++; \
	} while (0)

#define ZEND_HASH_FILL_END() \
		__fill_ht->nNumUsed = __fill_idx; \
		__fill_ht->nNumOfElements = __fill_idx; \
		__fill_ht->nNextFreeElement = __fill_idx; \
		__fill_ht->nInternalPointer = __fill_idx ? 0 : HT_INVALID_IDX; \
	} while (0)

/**
 * @description: 添加元素到哈希表后面
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串key
 * @param zval* zv 元素
 * @return: 
 */
static zend_always_inline zval *_zend_hash_append(HashTable *ht, zend_string *key, zval *zv)
{
	uint32_t idx = ht->nNumUsed++;
	uint32_t nIndex;
	Bucket *p = ht->arData + idx;

	ZVAL_COPY_VALUE(&p->val, zv);	//复制元素到指定位置
	if (!ZSTR_IS_INTERNED(key)) { //非内部字符串
		ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;	//取消静态key属性
		zend_string_addref(key);	//计数增加
		zend_string_hash_val(key);	//计算key哈希值
	}
	p->key = key;
	p->h = ZSTR_H(key);
	nIndex = (uint32_t)p->h | ht->nTableMask;
	Z_NEXT(p->val) = HT_HASH(ht, nIndex);
	HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(idx);
	ht->nNumUsed = idx + 1;
	ht->nNumOfElements++;
	return &p->val;
}

/**
 * @description: 添加指针元素到哈希表后面
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串key
 * @param void* ptr 指针元素
 * @return: 
 */
static zend_always_inline zval *_zend_hash_append_ptr(HashTable *ht, zend_string *key, void *ptr)
{
	uint32_t idx = ht->nNumUsed++;
	uint32_t nIndex;
	Bucket *p = ht->arData + idx;

	ZVAL_PTR(&p->val, ptr);
	if (!ZSTR_IS_INTERNED(key)) {
		ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
		zend_string_addref(key);
		zend_string_hash_val(key);
	}
	p->key = key;
	p->h = ZSTR_H(key);
	nIndex = (uint32_t)p->h | ht->nTableMask;
	Z_NEXT(p->val) = HT_HASH(ht, nIndex);
	HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(idx);
	ht->nNumUsed = idx + 1;
	ht->nNumOfElements++;
	return &p->val;
}

/**
 * @description: 添加间接元素到哈希表后面
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串key
 * @param zval* ptr 指针元素
 * @return: 
 */
static zend_always_inline void _zend_hash_append_ind(HashTable *ht, zend_string *key, zval *ptr)
{
	uint32_t idx = ht->nNumUsed++;
	uint32_t nIndex;
	Bucket *p = ht->arData + idx;

	ZVAL_INDIRECT(&p->val, ptr);
	if (!ZSTR_IS_INTERNED(key)) {
		ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
		zend_string_addref(key);
		zend_string_hash_val(key);
	}
	p->key = key;
	p->h = ZSTR_H(key);
	nIndex = (uint32_t)p->h | ht->nTableMask;
	Z_NEXT(p->val) = HT_HASH(ht, nIndex);
	HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(idx);
	ht->nNumUsed = idx + 1;
	ht->nNumOfElements++;
}

#endif							/* ZEND_HASH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
