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

#include "zend.h"
#include "zend_globals.h"
#include "zend_variables.h"

#if ZEND_DEBUG
# define HT_ASSERT(ht, expr) \
	ZEND_ASSERT((expr) || ((ht)->u.flags & HASH_FLAG_ALLOW_COW_VIOLATION))
#else
# define HT_ASSERT(ht, expr)
#endif

#define HT_ASSERT_RC1(ht) HT_ASSERT(ht, GC_REFCOUNT(ht) == 1)

#define HT_POISONED_PTR ((HashTable *) (intptr_t) -1)

#if ZEND_DEBUG

#define HT_OK					0x00
#define HT_IS_DESTROYING		0x40
#define HT_DESTROYED			0x80
#define HT_CLEANING				0xc0

/**
 * @description: 检查哈希表状态
 * @param HashTable* ht 哈希表指针
 * @param char* file 文件名
 * @param int line 行号
 * @return: 
 */
static void _zend_is_inconsistent(const HashTable *ht, const char *file, int line)
{
	//正常
	if (ht->u.v.consistency == HT_OK) {
		return;
	}
	switch (ht->u.v.consistency) {
		case HT_IS_DESTROYING:
			zend_output_debug_string(1, "%s(%d) : ht=%p is being destroyed", file, line, ht);
			break;
		case HT_DESTROYED:
			zend_output_debug_string(1, "%s(%d) : ht=%p is already destroyed", file, line, ht);
			break;
		case HT_CLEANING:
			zend_output_debug_string(1, "%s(%d) : ht=%p is being cleaned", file, line, ht);
			break;
		default:
			zend_output_debug_string(1, "%s(%d) : ht=%p is inconsistent", file, line, ht);
			break;
	}
	ZEND_ASSERT(0);
}
#define IS_CONSISTENT(a) _zend_is_inconsistent(a, __FILE__, __LINE__);
#define SET_INCONSISTENT(n) do { \
		(ht)->u.v.consistency = n; \
	} while (0)
#else
#define IS_CONSISTENT(a)
#define SET_INCONSISTENT(n)
#endif

#define HASH_PROTECT_RECURSION(ht)														\
	if ((ht)->u.flags & HASH_FLAG_APPLY_PROTECTION) {									\
		if (((ht)->u.flags & ZEND_HASH_APPLY_COUNT_MASK) >= (3 << 8)) {												\
			zend_error_noreturn(E_ERROR, "Nesting level too deep - recursive dependency?");\
		}																				\
		ZEND_HASH_INC_APPLY_COUNT(ht);													\
	}

#define HASH_UNPROTECT_RECURSION(ht)													\
	if ((ht)->u.flags & HASH_FLAG_APPLY_PROTECTION) {									\
		ZEND_HASH_DEC_APPLY_COUNT(ht);													\
	}

#define ZEND_HASH_IF_FULL_DO_RESIZE(ht)				\
	if ((ht)->nNumUsed >= (ht)->nTableSize) {		\
		zend_hash_do_resize(ht);					\
	}

static void ZEND_FASTCALL zend_hash_do_resize(HashTable *ht);

static zend_always_inline uint32_t zend_hash_check_size(uint32_t nSize)
{
#if defined(ZEND_WIN32)
	unsigned long index;
#endif

	/* Use big enough power of 2 */
	/* size should be between HT_MIN_SIZE and HT_MAX_SIZE */
	if (nSize < HT_MIN_SIZE) {
		nSize = HT_MIN_SIZE;
	} else if (UNEXPECTED(nSize >= HT_MAX_SIZE)) {
		zend_error_noreturn(E_ERROR, "Possible integer overflow in memory allocation (%u * %zu + %zu)", nSize, sizeof(Bucket), sizeof(Bucket));
	}

#if defined(ZEND_WIN32)
	if (BitScanReverse(&index, nSize - 1)) {
		return 0x2 << ((31 - index) ^ 0x1f);
	} else {
		/* nSize is ensured to be in the valid range, fall back to it
		   rather than using an undefined bis scan result. */
		return nSize;
	}
#elif (defined(__GNUC__) || __has_builtin(__builtin_clz))  && defined(PHP_HAVE_BUILTIN_CLZ)
	return 0x2 << (__builtin_clz(nSize - 1) ^ 0x1f);
#else
	nSize -= 1;
	nSize |= (nSize >> 1);
	nSize |= (nSize >> 2);
	nSize |= (nSize >> 4);
	nSize |= (nSize >> 8);
	nSize |= (nSize >> 16);
	return nSize + 1;
#endif
}

/**
 * @description:  哈希表真正的初始化操作，初始化bucket
 * @param HashTable* ht 哈希表指针
 * @param int packed 是否索引数组
 * @return: void
 */
static zend_always_inline void zend_hash_real_init_ex(HashTable *ht, int packed)
{
	HT_ASSERT_RC1(ht);
	ZEND_ASSERT(!((ht)->u.flags & HASH_FLAG_INITIALIZED)); //断言是否初始化

	//索引数组
	if (packed) {
		HT_SET_DATA_ADDR(ht, pemalloc(HT_SIZE(ht), (ht)->u.flags & HASH_FLAG_PERSISTENT)); 		//初始化bucket，并将位置指向第一个bucket
		(ht)->u.flags |= HASH_FLAG_INITIALIZED | HASH_FLAG_PACKED;	//标记哈希表已初始化并且为索引数组
		HT_HASH_RESET_PACKED(ht);	//将两个数组索引的值设置为-1
	} else {
		//枚举数组
		(ht)->nTableMask = -(ht)->nTableSize; //掩码的值总是哈希表大小的相反数，即 -(ht)->nTableSize
		HT_SET_DATA_ADDR(ht, pemalloc(HT_SIZE(ht), (ht)->u.flags & HASH_FLAG_PERSISTENT)); //初始化bucket，并将位置指向第一个bucket
		(ht)->u.flags |= HASH_FLAG_INITIALIZED; //标记哈希表为已经初始化

		//如果nTableMask = -8 将bucket的索引值设置为-1 （初始化bucket默认索引大小是8个）
		if (EXPECTED(ht->nTableMask == (uint32_t)-8)) {
			Bucket *arData = ht->arData;

			HT_HASH_EX(arData, -8) = -1;
			HT_HASH_EX(arData, -7) = -1;
			HT_HASH_EX(arData, -6) = -1;
			HT_HASH_EX(arData, -5) = -1;
			HT_HASH_EX(arData, -4) = -1;
			HT_HASH_EX(arData, -3) = -1;
			HT_HASH_EX(arData, -2) = -1;
			HT_HASH_EX(arData, -1) = -1;
		} else {
			//如果不等于 -1 ，将所有索引值设置为-1
			HT_HASH_RESET(ht);
		}
	}
}

/**
 * @description: 检测并初始化哈希表
 * @param HashTable* ht 哈希表指针
 * @param int 是否数字索引的哈希表
 * @return: 
 */
static zend_always_inline void zend_hash_check_init(HashTable *ht, int packed)
{
	HT_ASSERT_RC1(ht);
	if (UNEXPECTED(!((ht)->u.flags & HASH_FLAG_INITIALIZED))) {
		zend_hash_real_init_ex(ht, packed);
	}
}

#define CHECK_INIT(ht, packed) \
	zend_hash_check_init(ht, packed)

static const uint32_t uninitialized_bucket[-HT_MIN_MASK] =
	{HT_INVALID_IDX, HT_INVALID_IDX};

/**
 * @description: 初始化哈希表结构体
 * @param HashTable* ht 哈希表指针
 * @param uint32_t nSize 哈希表大小
 * @param dtor_func_t pDestructor 哈希表析构函数
 * @zend_bool persistent 是否永久存储
 * @return: 
 */
ZEND_API void ZEND_FASTCALL _zend_hash_init(HashTable *ht, uint32_t nSize, dtor_func_t pDestructor, zend_bool persistent ZEND_FILE_LINE_DC)
{
	GC_REFCOUNT(ht) = 1;
	GC_TYPE_INFO(ht) = IS_ARRAY | (persistent ? 0 : (GC_COLLECTABLE << GC_FLAGS_SHIFT));
	ht->u.flags = (persistent ? HASH_FLAG_PERSISTENT : 0) | HASH_FLAG_APPLY_PROTECTION | HASH_FLAG_STATIC_KEYS;
	ht->nTableMask = HT_MIN_MASK;
	HT_SET_DATA_ADDR(ht, &uninitialized_bucket);
	ht->nNumUsed = 0;
	ht->nNumOfElements = 0;
	ht->nInternalPointer = HT_INVALID_IDX;
	ht->nNextFreeElement = 0;
	ht->pDestructor = pDestructor;
	ht->nTableSize = zend_hash_check_size(nSize);
}

/**
 * @description: 扩展索引数组的哈希表, 如果超过最大限制，提示整数溢出
 * @param HashTable* ht 哈希表指针
 * @return: void
 */
static void ZEND_FASTCALL zend_hash_packed_grow(HashTable *ht)
{
	HT_ASSERT_RC1(ht);
	if (ht->nTableSize >= HT_MAX_SIZE) {
		zend_error_noreturn(E_ERROR, "Possible integer overflow in memory allocation (%u * %zu + %zu)", ht->nTableSize * 2, sizeof(Bucket), sizeof(Bucket));
	}
	ht->nTableSize += ht->nTableSize;
	HT_SET_DATA_ADDR(ht, perealloc2(HT_GET_DATA_ADDR(ht), HT_SIZE(ht), HT_USED_SIZE(ht), ht->u.flags & HASH_FLAG_PERSISTENT));
}

/**
 * @description: 哈希表初始化执行函数
 * @param HashTable* ht 哈希表指针
 * @param zend_bool packed 是否压缩（数字索引数组专用）
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_real_init(HashTable *ht, zend_bool packed)
{
	IS_CONSISTENT(ht);

	HT_ASSERT_RC1(ht);
	zend_hash_real_init_ex(ht, packed);
}

/**
 * @description: 索引数组哈希表转成枚举数组哈希表
 * @param HashTable* ht 索引数组哈希表指针
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_packed_to_hash(HashTable *ht)
{
	void *new_data, *old_data = HT_GET_DATA_ADDR(ht);
	Bucket *old_buckets = ht->arData;

	HT_ASSERT_RC1(ht);
	ht->u.flags &= ~HASH_FLAG_PACKED;
	new_data = pemalloc(HT_SIZE_EX(ht->nTableSize, -ht->nTableSize), (ht)->u.flags & HASH_FLAG_PERSISTENT);
	ht->nTableMask = -ht->nTableSize;
	HT_SET_DATA_ADDR(ht, new_data);
	memcpy(ht->arData, old_buckets, sizeof(Bucket) * ht->nNumUsed);
	pefree(old_data, (ht)->u.flags & HASH_FLAG_PERSISTENT);
	zend_hash_rehash(ht);
}
/**
 * @description: 枚举数组哈希表转成索引数组哈希表
 * @param HashTable* ht 枚举数组哈希表指针
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_to_packed(HashTable *ht)
{
	void *new_data, *old_data = HT_GET_DATA_ADDR(ht);
	Bucket *old_buckets = ht->arData;

	HT_ASSERT_RC1(ht);
	new_data = pemalloc(HT_SIZE_EX(ht->nTableSize, HT_MIN_MASK), (ht)->u.flags & HASH_FLAG_PERSISTENT);
	ht->u.flags |= HASH_FLAG_PACKED | HASH_FLAG_STATIC_KEYS;
	ht->nTableMask = HT_MIN_MASK;
	HT_SET_DATA_ADDR(ht, new_data);
	HT_HASH_RESET_PACKED(ht);
	memcpy(ht->arData, old_buckets, sizeof(Bucket) * ht->nNumUsed);
	pefree(old_data, (ht)->u.flags & HASH_FLAG_PERSISTENT);
}

/**
 * @description: 初始化哈希表
 * @param HashTable* ht 哈希表指针
 * @param uint32_t nSize 哈希表大小
 * @param dtor_func_t pDestructor 哈希表析构函数
 * @param zend_bool bApplyProtection 是否开启递归保护
 * @return: void
 */
ZEND_API void ZEND_FASTCALL _zend_hash_init_ex(HashTable *ht, uint32_t nSize, dtor_func_t pDestructor, zend_bool persistent, zend_bool bApplyProtection ZEND_FILE_LINE_DC)
{
	_zend_hash_init(ht, nSize, pDestructor, persistent ZEND_FILE_LINE_RELAY_CC);
	if (!bApplyProtection) {
		ht->u.flags &= ~HASH_FLAG_APPLY_PROTECTION;
	}
}

/**
 * @description: 扩展哈希表到指定大小
 * @param HashTable* ht 哈希表指针
 * @param uint32_t nSize 要扩展到的大小
 * @param zend_book packed 是否数字索引数组哈希表
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_extend(HashTable *ht, uint32_t nSize, zend_bool packed)
{
	HT_ASSERT_RC1(ht);
	if (nSize == 0) return;

	//未初始化
	if (UNEXPECTED(!((ht)->u.flags & HASH_FLAG_INITIALIZED))) {
		if (nSize > ht->nTableSize) {
			ht->nTableSize = zend_hash_check_size(nSize);
		}
		zend_hash_check_init(ht, packed);
	} else {

		//数字索引数组处理，直接扩展内存
		if (packed) {
			ZEND_ASSERT(ht->u.flags & HASH_FLAG_PACKED);
			if (nSize > ht->nTableSize) {
				ht->nTableSize = zend_hash_check_size(nSize);
				HT_SET_DATA_ADDR(ht, perealloc2(HT_GET_DATA_ADDR(ht), HT_SIZE(ht), HT_USED_SIZE(ht), ht->u.flags & HASH_FLAG_PERSISTENT));
			}
		} else {
			//枚举数组处理，直接复制一个新，然后重新计算hash处理
			ZEND_ASSERT(!(ht->u.flags & HASH_FLAG_PACKED));
			if (nSize > ht->nTableSize) {
				void *new_data, *old_data = HT_GET_DATA_ADDR(ht);
				Bucket *old_buckets = ht->arData;
				nSize = zend_hash_check_size(nSize);
				new_data = pemalloc(HT_SIZE_EX(nSize, -nSize), ht->u.flags & HASH_FLAG_PERSISTENT);
				ht->nTableSize = nSize;
				ht->nTableMask = -ht->nTableSize;
				HT_SET_DATA_ADDR(ht, new_data);
				memcpy(ht->arData, old_buckets, sizeof(Bucket) * ht->nNumUsed);
				pefree(old_data, ht->u.flags & HASH_FLAG_PERSISTENT);
				zend_hash_rehash(ht);
			}
		}
	}
}

/**
 * @description: 重新计算数组的元素数量，（标记为IS_UNDEF的除外）
 * @param HashTable* ht 哈希表指针
 * @return: uint32_t 元素数量
 */
static uint32_t zend_array_recalc_elements(HashTable *ht)
{
       zval *val;
       uint32_t num = ht->nNumOfElements;

	   ZEND_HASH_FOREACH_VAL(ht, val) {
		   if (Z_TYPE_P(val) == IS_INDIRECT) {
			   if (UNEXPECTED(Z_TYPE_P(Z_INDIRECT_P(val)) == IS_UNDEF)) {
				   num--;
			   }
		   }
       } ZEND_HASH_FOREACH_END();
       return num;
}
/* }}} */

/**
 * @description: 计算哈希表元素数量-数组大小
 * @param HashTable* ht 哈希表指针
 * @return: uint32_t 元素个数
 */
ZEND_API uint32_t zend_array_count(HashTable *ht)
{
	uint32_t num;
	//如果哈希表属性设置的是需要重新计算，则重新计算；如果元素中不存在标记为IS_UNDEF元素，设置属性为不需要重新计算
	if (UNEXPECTED(ht->u.v.flags & HASH_FLAG_HAS_EMPTY_IND)) {
		num = zend_array_recalc_elements(ht);
		if (UNEXPECTED(ht->nNumOfElements == num)) {
			ht->u.v.flags &= ~HASH_FLAG_HAS_EMPTY_IND;
		}
	} else if (UNEXPECTED(ht == &EG(symbol_table))) {
		//计算全局符号表的元素计数
		num = zend_array_recalc_elements(ht);
	} else {
		//计算实参的元素计数
		num = zend_hash_num_elements(ht);
	}
	return num;
}
/* }}} */

/**
 * @description: 设置或取消哈希表的递归保护属性
 * @param HashTable* ht 哈希表指针
 * @param zend_bool bApplyProtection 大于0 设置递归保护，等于0，取消递归保护
 * @return: 
 */
ZEND_API void ZEND_FASTCALL zend_hash_set_apply_protection(HashTable *ht, zend_bool bApplyProtection)
{
	if (bApplyProtection) {
		ht->u.flags |= HASH_FLAG_APPLY_PROTECTION;
	} else {
		ht->u.flags &= ~HASH_FLAG_APPLY_PROTECTION;
	}
}

/**
 * @description: 添加哈希表到执行时全局数组的迭代器的中
 * @param HashTable *ht 待插入的哈希表指针
 * @param HashPosition pos 待插入的位置
 * @return: 
 */
ZEND_API uint32_t ZEND_FASTCALL zend_hash_iterator_add(HashTable *ht, HashPosition pos)
{
	HashTableIterator *iter = EG(ht_iterators);
	HashTableIterator *end  = iter + EG(ht_iterators_count);
	uint32_t idx;

	if (EXPECTED(ht->u.v.nIteratorsCount != 255)) {
		ht->u.v.nIteratorsCount++;
	}

	//如果有空间，责直接插入并返回
	while (iter != end) {
		if (iter->ht == NULL) {
			iter->ht = ht;
			iter->pos = pos;
			idx = iter - EG(ht_iterators);
			if (idx + 1 > EG(ht_iterators_used)) {
				EG(ht_iterators_used) = idx + 1;
			}
			return idx;
		}
		iter++;
	}

	//如果没有空间了，责重新申请两倍的空间，并且复制老的迭代器数组过去
	if (EG(ht_iterators) == EG(ht_iterators_slots)) {
		EG(ht_iterators) = emalloc(sizeof(HashTableIterator) * (EG(ht_iterators_count) + 8));
		memcpy(EG(ht_iterators), EG(ht_iterators_slots), sizeof(HashTableIterator) * EG(ht_iterators_count));
	} else {
		//如果有空间，责直接扩容
		EG(ht_iterators) = erealloc(EG(ht_iterators), sizeof(HashTableIterator) * (EG(ht_iterators_count) + 8));
	}
	iter = EG(ht_iterators) + EG(ht_iterators_count);
	EG(ht_iterators_count) += 8;
	iter->ht = ht;
	iter->pos = pos;
	memset(iter + 1, 0, sizeof(HashTableIterator) * 7);
	idx = iter - EG(ht_iterators);
	EG(ht_iterators_used) = idx + 1;
	return idx;
}

/**
 * @description: 将哈希表插入对应的迭代器位置并返回pos
 * @param uint32_t idx 索引值
 * @param HashTable* ht 待插入哈希表
 * @return: HashPosition pos
 */
ZEND_API HashPosition ZEND_FASTCALL zend_hash_iterator_pos(uint32_t idx, HashTable *ht)
{
	HashTableIterator *iter = EG(ht_iterators) + idx;

	ZEND_ASSERT(idx != (uint32_t)-1);
	if (iter->pos == HT_INVALID_IDX) {
		return HT_INVALID_IDX;
	} else if (UNEXPECTED(iter->ht != ht)) {
		if (EXPECTED(iter->ht) && EXPECTED(iter->ht != HT_POISONED_PTR)
				&& EXPECTED(iter->ht->u.v.nIteratorsCount != 255)) {
			iter->ht->u.v.nIteratorsCount--;
		}
		if (EXPECTED(ht->u.v.nIteratorsCount != 255)) {
			ht->u.v.nIteratorsCount++;
		}
		iter->ht = ht;
		iter->pos = ht->nInternalPointer;
	}
	return iter->pos;
}

/**
 * @description: 将zval插入到索引位置，并返回pos
 * @param uint32_t idx 索引值
 * @param zval* array 待插入哈希表
 * @return: HashPosition pos
 */
ZEND_API HashPosition ZEND_FASTCALL zend_hash_iterator_pos_ex(uint32_t idx, zval *array)
{
	HashTable *ht = Z_ARRVAL_P(array);
	HashTableIterator *iter = EG(ht_iterators) + idx;

	ZEND_ASSERT(idx != (uint32_t)-1);
	if (iter->pos == HT_INVALID_IDX) {
		return HT_INVALID_IDX;
	} else if (UNEXPECTED(iter->ht != ht)) {
		if (EXPECTED(iter->ht) && EXPECTED(iter->ht != HT_POISONED_PTR)
				&& EXPECTED(iter->ht->u.v.nIteratorsCount != 255)) {
			iter->ht->u.v.nIteratorsCount--;
		}
		SEPARATE_ARRAY(array);
		ht = Z_ARRVAL_P(array);
		if (EXPECTED(ht->u.v.nIteratorsCount != 255)) {
			ht->u.v.nIteratorsCount++;
		}
		iter->ht = ht;
		iter->pos = ht->nInternalPointer;
	}
	return iter->pos;
}


/**
 * @description: 根据索引删除全局迭代器的值，只是赋值为NULL
 * @param uint32_t  idx 索引值
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_iterator_del(uint32_t idx)
{
	HashTableIterator *iter = EG(ht_iterators) + idx;

	ZEND_ASSERT(idx != (uint32_t)-1);

	if (EXPECTED(iter->ht) && EXPECTED(iter->ht != HT_POISONED_PTR)
			&& EXPECTED(iter->ht->u.v.nIteratorsCount != 255)) {
		iter->ht->u.v.nIteratorsCount--;
	}
	iter->ht = NULL;

	if (idx == EG(ht_iterators_used) - 1) {
		while (idx > 0 && EG(ht_iterators)[idx - 1].ht == NULL) {
			idx--;
		}
		EG(ht_iterators_used) = idx;
	}
}

/**
 * @description: 从迭代器中删除一个哈希表
 * @param HashTable* ht 要删除的哈希表
 * @return: void
 */
static zend_never_inline void ZEND_FASTCALL _zend_hash_iterators_remove(HashTable *ht)
{
	HashTableIterator *iter = EG(ht_iterators);
	HashTableIterator *end  = iter + EG(ht_iterators_used);

	while (iter != end) {
		if (iter->ht == ht) {
			iter->ht = HT_POISONED_PTR;
		}
		iter++;
	}
}

/**
 * @description: 从迭代器中删除一个计数大于0的哈希表
 * @param HashTable* ht 待删除的哈希表
 * @return: void
 */
static zend_always_inline void zend_hash_iterators_remove(HashTable *ht)
{
	if (UNEXPECTED(ht->u.v.nIteratorsCount)) {
		_zend_hash_iterators_remove(ht);
	}
}

ZEND_API HashPosition ZEND_FASTCALL zend_hash_iterators_lower_pos(HashTable *ht, HashPosition start)
{
	HashTableIterator *iter = EG(ht_iterators);
	HashTableIterator *end  = iter + EG(ht_iterators_used);
	HashPosition res = HT_INVALID_IDX;

	while (iter != end) {
		if (iter->ht == ht) {
			if (iter->pos >= start && iter->pos < res) {
				res = iter->pos;
			}
		}
		iter++;
	}
	return res;
}

/**
 * @description: 将哈希表迭代器的位置从from更新成to
 * @param HashTable* ht 待更新的哈希表
 * @param HashPosition from 原坐标
 * @param HashPosition to	新坐标
 * @return: 
 */
ZEND_API void ZEND_FASTCALL _zend_hash_iterators_update(HashTable *ht, HashPosition from, HashPosition to)
{
	HashTableIterator *iter = EG(ht_iterators);
	HashTableIterator *end  = iter + EG(ht_iterators_used);

	while (iter != end) {
		if (iter->ht == ht && iter->pos == from) {
			iter->pos = to;
		}
		iter++;
	}
}

/**
 * @description: 哈希表查找函数,根据zend_string查找
 * @param HashTable* ht
 * @param zend_string* key
 * @return Bucket|null 查找到返回Bucket 否则返回NULL
 */
static zend_always_inline Bucket *zend_hash_find_bucket(const HashTable *ht, zend_string *key)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p, *arData;

	h = zend_string_hash_val(key); //获取字符串哈希值
	arData = ht->arData;
	nIndex = h | ht->nTableMask;	//计算索引区位置（solt下标）
	idx = HT_HASH_EX(arData, nIndex);	//根据索引去下标获取到存储区的下标
	while (EXPECTED(idx != HT_INVALID_IDX)) {
		p = HT_HASH_TO_BUCKET_EX(arData, idx);	//获取存储的数据
		if (EXPECTED(p->key == key)) { /* check for the same interned string 比较key和存储数据，相等即找到就返回*/
			return p;
		} else if (EXPECTED(p->h == h) &&	//否则对比哈希值，对比长度，对比内存。如果都匹配成功，则找到并返回
		     EXPECTED(p->key) &&
		     EXPECTED(ZSTR_LEN(p->key) == ZSTR_LEN(key)) &&
		     EXPECTED(memcmp(ZSTR_VAL(p->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0)) {
			return p;
		}
		idx = Z_NEXT(p->val);	//根据zval的.u2.next查找下一个存储位置的数据，这里用于哈希冲突查找
	}
	return NULL;
}

/**
 * @description: 哈希表查找函数，根据字符串的值，长度，哈希值查找
 * @param HashTable* ht
 * @param char* str
 * @param size_t len
 * @param zend_ulong h
 * @return Bucket|null 查找到返回Bucket 否则返回NULL
 */
static zend_always_inline Bucket *zend_hash_str_find_bucket(const HashTable *ht, const char *str, size_t len, zend_ulong h)
{
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p, *arData;

	arData = ht->arData;
	nIndex = h | ht->nTableMask;
	idx = HT_HASH_EX(arData, nIndex);
	while (idx != HT_INVALID_IDX) {
		ZEND_ASSERT(idx < HT_IDX_TO_HASH(ht->nTableSize));
		p = HT_HASH_TO_BUCKET_EX(arData, idx);
		if ((p->h == h)
			 && p->key
			 && (ZSTR_LEN(p->key) == len)
			 && !memcmp(ZSTR_VAL(p->key), str, len)) {
			return p;
		}
		idx = Z_NEXT(p->val);
	}
	return NULL;
}

/**
 * @description: 哈希表查找函数，根据哈希值查找
 * @param HashTable* ht
 * @param zend_ulong h
 * @return Bucket|null 查找到返回Bucket 否则返回NULL
 */
static zend_always_inline Bucket *zend_hash_index_find_bucket(const HashTable *ht, zend_ulong h)
{
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p, *arData;

	arData = ht->arData;
	nIndex = h | ht->nTableMask;
	idx = HT_HASH_EX(arData, nIndex);
	while (idx != HT_INVALID_IDX) {
		ZEND_ASSERT(idx < HT_IDX_TO_HASH(ht->nTableSize));
		p = HT_HASH_TO_BUCKET_EX(arData, idx);
		if (p->h == h && !p->key) {
			return p;
		}
		idx = Z_NEXT(p->val);
	}
	return NULL;
}

/**
 * @description: 向枚举哈希表增加或更新一个元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 待操作key
 * @param zval* pData 待写入数据
 * @param uint32_t flag 操作掩码
 * @return: 
 */
static zend_always_inline zval *_zend_hash_add_or_update_i(HashTable *ht, zend_string *key, zval *pData, uint32_t flag ZEND_FILE_LINE_DC)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;

	//检测状态和gc
	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (UNEXPECTED(!(ht->u.flags & HASH_FLAG_INITIALIZED))) {
		//如果哈希表未初始化，则初始化，并且跳转到添加操作
		CHECK_INIT(ht, 0);
		goto add_to_hash;
	} else if (ht->u.flags & HASH_FLAG_PACKED) {
		//如果是压缩数组哈希表，则调用压缩数组专用处理函数
		zend_hash_packed_to_hash(ht);
	} else if ((flag & HASH_ADD_NEW) == 0) {
		p = zend_hash_find_bucket(ht, key);	//根据key找到哈希表的存储数据

		if (p) {
			zval *data;

			//添加操作
			if (flag & HASH_ADD) {
				if (!(flag & HASH_UPDATE_INDIRECT)) {
					return NULL;
				}
				ZEND_ASSERT(&p->val != pData);
				data = &p->val;
				if (Z_TYPE_P(data) == IS_INDIRECT) {	//如果是间接的zval，则判断指向真实的zval是否为IS_UNDEF，是则返回NULL
					data = Z_INDIRECT_P(data);
					if (Z_TYPE_P(data) != IS_UNDEF) {
						return NULL;
					}
				} else {
					return NULL;
				}
			} else {
				//更新操作
				ZEND_ASSERT(&p->val != pData);
				data = &p->val;
				if ((flag & HASH_UPDATE_INDIRECT) && Z_TYPE_P(data) == IS_INDIRECT) {
					//如果是更新间接数据操作，并且数据的类型是间接类型
					data = Z_INDIRECT_P(data);
				}
			}
			if (ht->pDestructor) {
				ht->pDestructor(data);	//调用析构函数处理
			}
			ZVAL_COPY_VALUE(data, pData);	//更新并返回
			return data;
		}
	}

	ZEND_HASH_IF_FULL_DO_RESIZE(ht);		/* If the Hash table is full, resize it */
//添加操作
add_to_hash:
	idx = ht->nNumUsed++;
	ht->nNumOfElements++;
	if (ht->nInternalPointer == HT_INVALID_IDX) {
		ht->nInternalPointer = idx; //遍历指针
	}
	zend_hash_iterators_update(ht, HT_INVALID_IDX, idx);
	p = ht->arData + idx;
	p->key = key;
	if (!ZSTR_IS_INTERNED(key)) { //非内部字符串，计算gc和hash值
		zend_string_addref(key);
		ht->u.flags &= ~HASH_FLAG_STATIC_KEYS;
		zend_string_hash_val(key);
	}
	p->h = h = ZSTR_H(key);
	ZVAL_COPY_VALUE(&p->val, pData);
	nIndex = h | ht->nTableMask;
	Z_NEXT(p->val) = HT_HASH(ht, nIndex);
	HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(idx);

	return &p->val;
}

/**
 * @description: 向枚举哈希表添加或更新元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串的key
 * @param zval* pData 对应的数据
 * @param uint32_t flat 操作掩码
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_add_or_update(HashTable *ht, zend_string *key, zval *pData, uint32_t flag ZEND_FILE_LINE_DC)
{
	return _zend_hash_add_or_update_i(ht, key, pData, flag ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description: 向枚举哈希表添加元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串的key
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_add(HashTable *ht, zend_string *key, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_add_or_update_i(ht, key, pData, HASH_ADD ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description: 向枚举哈希表更新
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串的key
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_update(HashTable *ht, zend_string *key, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_add_or_update_i(ht, key, pData, HASH_UPDATE ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description: 向枚举哈希表的间接数据更新
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串的key
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_update_ind(HashTable *ht, zend_string *key, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_add_or_update_i(ht, key, pData, HASH_UPDATE | HASH_UPDATE_INDIRECT ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description: 添加新的数据到向枚举哈希表
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 字符串的key
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_add_new(HashTable *ht, zend_string *key, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_add_or_update_i(ht, key, pData, HASH_ADD_NEW ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description: 添加或更新字符串key到向枚举哈希表
 * @param HashTable* ht 待操作哈希表
 * @param char* str 字符串的key
 * @param size_t len 字符串长度
 * @param zval* pData 对应的数据
 * @param uint32_t flag 操作掩码
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_add_or_update(HashTable *ht, const char *str, size_t len, zval *pData, uint32_t flag ZEND_FILE_LINE_DC)
{
	zend_string *key = zend_string_init(str, len, ht->u.flags & HASH_FLAG_PERSISTENT);
	zval *ret = _zend_hash_add_or_update_i(ht, key, pData, flag ZEND_FILE_LINE_RELAY_CC);
	zend_string_release(key);
	return ret;
}

/**
 * @description: 更新字符串key到向枚举哈希表
 * @param HashTable* ht 待操作哈希表
 * @param char* str 字符串的key
 * @param size_t len 字符串长度
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_update(HashTable *ht, const char *str, size_t len, zval *pData ZEND_FILE_LINE_DC)
{
	zend_string *key = zend_string_init(str, len, ht->u.flags & HASH_FLAG_PERSISTENT);
	zval *ret = _zend_hash_add_or_update_i(ht, key, pData, HASH_UPDATE ZEND_FILE_LINE_RELAY_CC);
	zend_string_release(key);
	return ret;
}

/**
 * @description: 向枚举哈希表的间接数据更新，通过字符串key
 * @param HashTable* ht 待操作哈希表
 * @param char* str 字符串的key
 * @param size_t len 字符串长度
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_update_ind(HashTable *ht, const char *str, size_t len, zval *pData ZEND_FILE_LINE_DC)
{
	zend_string *key = zend_string_init(str, len, ht->u.flags & HASH_FLAG_PERSISTENT);
	zval *ret = _zend_hash_add_or_update_i(ht, key, pData, HASH_UPDATE | HASH_UPDATE_INDIRECT ZEND_FILE_LINE_RELAY_CC);
	zend_string_release(key);
	return ret;
}

/**
 * @description: 添加字符串key到向枚举哈希表
 * @param HashTable* ht 待操作哈希表
 * @param char* str 字符串的key
 * @param size_t len 字符串长度
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_add(HashTable *ht, const char *str, size_t len, zval *pData ZEND_FILE_LINE_DC)
{
	zend_string *key = zend_string_init(str, len, ht->u.flags & HASH_FLAG_PERSISTENT);
	zval *ret = _zend_hash_add_or_update_i(ht, key, pData, HASH_ADD ZEND_FILE_LINE_RELAY_CC);
	zend_string_release(key);
	return ret;
}

/**
 * @description: 添加新的字符串key到向枚举哈希表
 * @param HashTable* ht 待操作哈希表
 * @param char* str 字符串的key
 * @param size_t len 字符串长度
 * @param zval* pData 对应的数据
 * @return: zval
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_str_add_new(HashTable *ht, const char *str, size_t len, zval *pData ZEND_FILE_LINE_DC)
{
	zend_string *key = zend_string_init(str, len, ht->u.flags & HASH_FLAG_PERSISTENT);
	zval *ret = _zend_hash_add_or_update_i(ht, key, pData, HASH_ADD_NEW ZEND_FILE_LINE_RELAY_CC);
	zend_string_delref(key);
	return ret;
}

/**
 * @description: 向数值索引哈希表的特定位置新增一个空元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_ulong h 待操作哈希表
 * @return: 
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_index_add_empty_element(HashTable *ht, zend_ulong h)
{
	zval dummy;

	ZVAL_NULL(&dummy);
	return zend_hash_index_add(ht, h, &dummy);
}

/**
 * @description: 向哈希表的字符串key新增一个空元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_ulong h 待操作哈希表
 * @return: 
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_add_empty_element(HashTable *ht, zend_string *key)
{
	zval dummy;

	ZVAL_NULL(&dummy);
	return zend_hash_add(ht, key, &dummy);
}

/**
 * @description: 向哈希表的字符串key新增一个空元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_ulong h 待操作哈希表
 * @return: 
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_str_add_empty_element(HashTable *ht, const char *str, size_t len)
{
	zval dummy;

	ZVAL_NULL(&dummy);
	return zend_hash_str_add(ht, str, len, &dummy);
}

/**
 * @description: 对数值索引哈希表添加或更新一个元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_ulong h 待操作哈希表
 * @param zval* pData 待写入的数据
 * @param uint32_t flag 操作掩码
 * @return: zval
 */
static zend_always_inline zval *_zend_hash_index_add_or_update_i(HashTable *ht, zend_ulong h, zval *pData, uint32_t flag ZEND_FILE_LINE_DC)
{
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (UNEXPECTED(!(ht->u.flags & HASH_FLAG_INITIALIZED))) {
		//如哈希表未初始化，初始化，并添加数值索引哈希表的特定位置
		CHECK_INIT(ht, h < ht->nTableSize);
		if (h < ht->nTableSize) {
			//如果初始化的哈希表是数值索引，则走插入数值索引操作，否则走字符串索引操作
			p = ht->arData + h;
			goto add_to_packed;
		}
		goto add_to_hash;
	} else if (ht->u.flags & HASH_FLAG_PACKED) {
		//已初始化，并且哈希表是数值索引，并且插入的位置小于已用元素数量
		if (h < ht->nNumUsed) {
			p = ht->arData + h;
			if (Z_TYPE(p->val) != IS_UNDEF) {
				//对应位置的数据未标识IS_UNDEF，如果是添加操作，则返回NULL
				if (flag & HASH_ADD) {
					return NULL;
				}
				if (ht->pDestructor) {
					ht->pDestructor(&p->val);
				}
				ZVAL_COPY_VALUE(&p->val, pData);	//拷贝数据到对应位置上，并返回
				return &p->val;
			} else { /* we have to keep the order :( */
				//如果位置的元素已标记为IS_UNDEF，必须要将数值索引哈希表转换成枚举哈希表
				goto convert_to_hash;
			}
		} else if (EXPECTED(h < ht->nTableSize)) {
			//如果带插入索引大于等于已用元素数量，并且小于哈希表容量，则取到哈希表对应索引的数据块
			p = ht->arData + h;
		} else if ((h >> 1) < ht->nTableSize &&
		           (ht->nTableSize >> 1) < ht->nNumOfElements) {
					   //否则，如果插入位置小于哈希表容量的2倍，并且有效元素数大于等于容量的一半，触发扩容哈希表操作
			zend_hash_packed_grow(ht);
			p = ht->arData + h;
		} else {
			//否则触发转换枚举哈希表操作
			goto convert_to_hash;
		}

add_to_packed:
		/* incremental initialization of empty Buckets */
		//添加数据到数值索引哈希表
		if ((flag & (HASH_ADD_NEW|HASH_ADD_NEXT)) == (HASH_ADD_NEW|HASH_ADD_NEXT)) {
			ht->nNumUsed = h + 1;
		} else if (h >= ht->nNumUsed) {
			if (h > ht->nNumUsed) {
				Bucket *q = ht->arData + ht->nNumUsed;
				while (q != p) {
					ZVAL_UNDEF(&q->val);
					q++;
				}
			}
			ht->nNumUsed = h + 1;
		}
		ht->nNumOfElements++;
		if (ht->nInternalPointer == HT_INVALID_IDX) {
			//设置内部指针
			ht->nInternalPointer = h;
		}
		zend_hash_iterators_update(ht, HT_INVALID_IDX, h);	//更新哈希表迭代器位置
		if ((zend_long)h >= (zend_long)ht->nNextFreeElement) {
			//设置下一个插入元素的坐标
			ht->nNextFreeElement = h < ZEND_LONG_MAX ? h + 1 : ZEND_LONG_MAX;
		}
		p->h = h;
		p->key = NULL;
		ZVAL_COPY_VALUE(&p->val, pData);	//拷贝数据到对应索引上

		return &p->val;

convert_to_hash:
		zend_hash_packed_to_hash(ht);	//数值索引哈希表转换成枚举哈希表
	} else if ((flag & HASH_ADD_NEW) == 0) {
		p = zend_hash_index_find_bucket(ht, h);	//查找
		if (p) {
			if (flag & HASH_ADD) {
				return NULL;	//拒绝添加操作
			}
			ZEND_ASSERT(&p->val != pData);
			if (ht->pDestructor) {
				ht->pDestructor(&p->val);
			}
			ZVAL_COPY_VALUE(&p->val, pData);	//拷贝数据到对应位置
			if ((zend_long)h >= (zend_long)ht->nNextFreeElement) {
				ht->nNextFreeElement = h < ZEND_LONG_MAX ? h + 1 : ZEND_LONG_MAX; //设置下一个插入的数字索引
			}
			return &p->val;
		}
	}

	//如果哈希表空间已满，则触发扩容
	ZEND_HASH_IF_FULL_DO_RESIZE(ht);		/* If the Hash table is full, resize it */

add_to_hash:
	//添加数据到字符串索引哈希表
	idx = ht->nNumUsed++;
	ht->nNumOfElements++;
	if (ht->nInternalPointer == HT_INVALID_IDX) {
		ht->nInternalPointer = idx;
	}
	zend_hash_iterators_update(ht, HT_INVALID_IDX, idx);
	if ((zend_long)h >= (zend_long)ht->nNextFreeElement) {
		ht->nNextFreeElement = h < ZEND_LONG_MAX ? h + 1 : ZEND_LONG_MAX;
	}
	p = ht->arData + idx;
	p->h = h;
	p->key = NULL;
	nIndex = h | ht->nTableMask;	//计算映射表位置
	ZVAL_COPY_VALUE(&p->val, pData);	//复制数据
	Z_NEXT(p->val) = HT_HASH(ht, nIndex);	//旧映射表赋值给新数据的u2.next属性
	HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(idx); //将新索引赋值给映射表

	return &p->val;
}

/**
 * @description 向数值索引哈希表新增或更新数据
 * @param HashTable* ht 待操作的哈希表
 * @param  zend_ulong h 索引位置
 * @param zval* pData 待保存的数据
 * @param uint32_t flag 操作掩码
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_add_or_update(HashTable *ht, zend_ulong h, zval *pData, uint32_t flag ZEND_FILE_LINE_DC)
{
	return _zend_hash_index_add_or_update_i(ht, h, pData, flag ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description 向数值索引哈希表新增数据
 * @param HashTable* ht 待操作的哈希表
 * @param  zend_ulong h 索引位置
 * @param zval* pData 待保存的数据
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_add(HashTable *ht, zend_ulong h, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_index_add_or_update_i(ht, h, pData, HASH_ADD ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description 向数值索引哈希表新增数据
 * @param HashTable* ht 待操作的哈希表
 * @param  zend_ulong h 索引位置
 * @param zval* pData 待保存的数据
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_add_new(HashTable *ht, zend_ulong h, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_index_add_or_update_i(ht, h, pData, HASH_ADD | HASH_ADD_NEW ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description 向数值索引哈希表更新数据
 * @param HashTable* ht 待操作的哈希表
 * @param  zend_ulong h 索引位置
 * @param zval* pData 待保存的数据
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_update(HashTable *ht, zend_ulong h, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_index_add_or_update_i(ht, h, pData, HASH_UPDATE ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description 向数值索引哈希表的尾部插入数据
 * @param HashTable* ht 待操作的哈希表
 * @param zval* pData 待保存的数据
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_next_index_insert(HashTable *ht, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_index_add_or_update_i(ht, ht->nNextFreeElement, pData, HASH_ADD | HASH_ADD_NEXT ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description 向数值索引哈希表的尾部插入数据
 * @param HashTable* ht 待操作的哈希表
 * @param zval* pData 待保存的数据
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_next_index_insert_new(HashTable *ht, zval *pData ZEND_FILE_LINE_DC)
{
	return _zend_hash_index_add_or_update_i(ht, ht->nNextFreeElement, pData, HASH_ADD | HASH_ADD_NEW | HASH_ADD_NEXT ZEND_FILE_LINE_RELAY_CC);
}

/**
 * @description: 扩容哈希表
 * @param HashTable* ht
 * @return: void
 */
static void ZEND_FASTCALL zend_hash_do_resize(HashTable *ht)
{

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (ht->nNumUsed > ht->nNumOfElements + (ht->nNumOfElements >> 5)) { /* additional term is there to amortize the cost of compaction */
		//触发rehash操作
		zend_hash_rehash(ht);
	} else if (ht->nTableSize < HT_MAX_SIZE) {	/* Let's double the table size */
		void *new_data, *old_data = HT_GET_DATA_ADDR(ht);
		uint32_t nSize = ht->nTableSize + ht->nTableSize;
		Bucket *old_buckets = ht->arData;
		//申请两倍原哈希表的内存空间
		new_data = pemalloc(HT_SIZE_EX(nSize, -nSize), ht->u.flags & HASH_FLAG_PERSISTENT);
		ht->nTableSize = nSize;
		ht->nTableMask = -ht->nTableSize;
		HT_SET_DATA_ADDR(ht, new_data);
		memcpy(ht->arData, old_buckets, sizeof(Bucket) * ht->nNumUsed);	//拷贝原哈希表数据到新哈希表
		pefree(old_data, ht->u.flags & HASH_FLAG_PERSISTENT);	//释放原哈希表内存
		zend_hash_rehash(ht);	//rehash新的哈希表
	} else {
		zend_error_noreturn(E_ERROR, "Possible integer overflow in memory allocation (%u * %zu + %zu)", ht->nTableSize * 2, sizeof(Bucket) + sizeof(uint32_t), sizeof(Bucket));
	}
}

/**
 * @description: 对哈希表进行重新计算哈希索引
 * @param HashTable* ht 待操作的哈希表
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_rehash(HashTable *ht)
{
	Bucket *p;
	uint32_t nIndex, i;

	IS_CONSISTENT(ht);

	//有效元素数量为0，并且哈希表已初始化，则重置映射表为无索引
	if (UNEXPECTED(ht->nNumOfElements == 0)) {
		if (ht->u.flags & HASH_FLAG_INITIALIZED) {
			ht->nNumUsed = 0;
			HT_HASH_RESET(ht);	//重置映射表
		}
		return SUCCESS;
	}

	HT_HASH_RESET(ht); //重置映射表
	i = 0;
	p = ht->arData;
	if (HT_IS_WITHOUT_HOLES(ht)) {
		//如果数组没有IS_UNDEF元素，即所有元素有效
		do {
			nIndex = p->h | ht->nTableMask;	//计算哈希值
			Z_NEXT(p->val) = HT_HASH(ht, nIndex); //将数据的zval.u2.next更新为旧映射表的索引
			HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(i);	//更新数值索引到映射表
			p++;
		} while (++i < ht->nNumUsed);
	} else {
		//如果哈希表中有元素标记为IS_UNDEF
		do {
			if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) {
				uint32_t j = i;
				Bucket *q = p;

				if (EXPECTED(ht->u.v.nIteratorsCount == 0)) {
					//重新计算哈希索引
					while (++i < ht->nNumUsed) {
						p++;
						if (EXPECTED(Z_TYPE_INFO(p->val) != IS_UNDEF)) {
							ZVAL_COPY_VALUE(&q->val, &p->val);
							q->h = p->h;
							nIndex = q->h | ht->nTableMask;
							q->key = p->key;
							Z_NEXT(q->val) = HT_HASH(ht, nIndex);
							HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(j);
							if (UNEXPECTED(ht->nInternalPointer == i)) {
								ht->nInternalPointer = j;
							}
							q++;
							j++;
						}
					}
				} else {
					uint32_t iter_pos = zend_hash_iterators_lower_pos(ht, 0);

					while (++i < ht->nNumUsed) {
						p++;
						if (EXPECTED(Z_TYPE_INFO(p->val) != IS_UNDEF)) {
							ZVAL_COPY_VALUE(&q->val, &p->val);
							q->h = p->h;
							nIndex = q->h | ht->nTableMask;
							q->key = p->key;
							Z_NEXT(q->val) = HT_HASH(ht, nIndex);
							HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(j);
							if (UNEXPECTED(ht->nInternalPointer == i)) {
								ht->nInternalPointer = j;
							}
							if (UNEXPECTED(i >= iter_pos)) {
								do {
									zend_hash_iterators_update(ht, iter_pos, j);
									iter_pos = zend_hash_iterators_lower_pos(ht, iter_pos + 1);
								} while (iter_pos < i);
							}
							q++;
							j++;
						}
					}
				}
				ht->nNumUsed = j;
				break;
			}
			nIndex = p->h | ht->nTableMask;
			Z_NEXT(p->val) = HT_HASH(ht, nIndex);
			HT_HASH(ht, nIndex) = HT_IDX_TO_HASH(i);
			p++;
		} while (++i < ht->nNumUsed);
	}
	return SUCCESS;
}

/**
 * @description: 删除哈希表中的元素（标记为IS_UNDEF）
 * @param HashTable* ht待操作的哈希表
 * @param uint32_t idx 元素索引
 * @param Bucket* p 元素指针
 * @param Bucket* prev 前一个元素的指针
 * @return: void
 */
static zend_always_inline void _zend_hash_del_el_ex(HashTable *ht, uint32_t idx, Bucket *p, Bucket *prev)
{
	//非数值索引哈希表，把将要删除元素后面的数据前移
	if (!(ht->u.flags & HASH_FLAG_PACKED)) {
		if (prev) {
			Z_NEXT(prev->val) = Z_NEXT(p->val);	//待删除后面的元素前移
		} else {
			HT_HASH(ht, p->h | ht->nTableMask) = Z_NEXT(p->val);	//更新映射表为后面的元素
		}
	}
	if (HT_IDX_TO_HASH(ht->nNumUsed - 1) == idx) {
		//设置nNumUsed的值为：从数组开始到最后一个有效元素的个数
		do {
			ht->nNumUsed--;
		} while (ht->nNumUsed > 0 && (UNEXPECTED(Z_TYPE(ht->arData[ht->nNumUsed-1].val) == IS_UNDEF)));
	}
	ht->nNumOfElements--;		//减小有效元素数

	//如果待删除索引是哈希表内部指针，或者迭代计数不为0
	if (HT_IDX_TO_HASH(ht->nInternalPointer) == idx || UNEXPECTED(ht->u.v.nIteratorsCount)) {
		uint32_t new_idx;

		new_idx = idx = HT_HASH_TO_IDX(idx);	//新索引

		//while的作用是，从待删除的索引开始向后找到第一个未标记IS_UNDEF的数据坐标
		while (1) {
			new_idx++;
			if (new_idx >= ht->nNumUsed) {	//如果新索引大于哈希表的已用数量，设置哈新索引为无效值
				new_idx = HT_INVALID_IDX;
				break;
			} else if (Z_TYPE(ht->arData[new_idx].val) != IS_UNDEF) {	//如果新索引的数据不是IS_UNDEF则停止
				break;
			}
		}
		if (ht->nInternalPointer == idx) {
			ht->nInternalPointer = new_idx;	//如果内部指针位等于删除索引，将指针设置为新有效数据的坐标
		}
		zend_hash_iterators_update(ht, idx, new_idx);	//更新迭代位idx为new_idx（最新有效数据的坐标）
	}
	if (p->key) {
		zend_string_release(p->key); //释放p数据的key内存
	}
	if (ht->pDestructor) {
		zval tmp;
		ZVAL_COPY_VALUE(&tmp, &p->val);	//复制p到临时变量
		ZVAL_UNDEF(&p->val);	//设置p为IS_UNDEF
		ht->pDestructor(&tmp);	//析构函数处理临时变量
	} else {
		ZVAL_UNDEF(&p->val);	//直接设置p的值为IS_UNDEF
	}
}

/**
 * @description: 删除哈希表中索引位置的元素
 * @param HashTable* ht 待操作哈希表
 * @param uint32_t idx 索引
 * @param Bucket* p 待删除元素
 * @return: void
 */
static zend_always_inline void _zend_hash_del_el(HashTable *ht, uint32_t idx, Bucket *p)
{
	Bucket *prev = NULL;

	if (!(ht->u.flags & HASH_FLAG_PACKED)) {
		uint32_t nIndex = p->h | ht->nTableMask;	//映射表索引
		uint32_t i = HT_HASH(ht, nIndex);	//数据表索引

		//找到bucket的前一个
		if (i != idx) {
			prev = HT_HASH_TO_BUCKET(ht, i);
			while (Z_NEXT(prev->val) != idx) {
				i = Z_NEXT(prev->val);
				prev = HT_HASH_TO_BUCKET(ht, i);
			}
	 	}
	}

	//执行删除函数
	_zend_hash_del_el_ex(ht, idx, p, prev);
}

/**
 * @description: 从哈希表中删除一个元素
 * @param HashTable* ht
 * @param Bucket* p
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_del_bucket(HashTable *ht, Bucket *p)
{
	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);
	_zend_hash_del_el(ht, HT_IDX_TO_HASH(p - ht->arData), p);
}

/**
 * @description: 从哈希表中删除元素-字符串key
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 待删除字符串
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_del(HashTable *ht, zend_string *key)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;
	Bucket *prev = NULL;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	h = zend_string_hash_val(key);	//字符串哈希值
	nIndex = h | ht->nTableMask;	//映射表坐标

	idx = HT_HASH(ht, nIndex);	//数据表坐标
	while (idx != HT_INVALID_IDX) {
		p = HT_HASH_TO_BUCKET(ht, idx);	//找到元素
		if ((p->key == key) ||
			(p->h == h &&
		     p->key &&
		     ZSTR_LEN(p->key) == ZSTR_LEN(key) &&
		     memcmp(ZSTR_VAL(p->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0)) {
			_zend_hash_del_el_ex(ht, idx, p, prev);	//对比通过之后调用删除函数删除元素
			return SUCCESS;
		}
		prev = p;
		idx = Z_NEXT(p->val);
	}
	return FAILURE;
}


/**
 * @description: 从哈希表中删除间接元素-字符串key
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* key 待删除字符串
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_del_ind(HashTable *ht, zend_string *key)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;
	Bucket *prev = NULL;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	h = zend_string_hash_val(key);
	nIndex = h | ht->nTableMask;

	idx = HT_HASH(ht, nIndex);
	while (idx != HT_INVALID_IDX) {
		p = HT_HASH_TO_BUCKET(ht, idx);	//找到数据
		if ((p->key == key) ||
			(p->h == h &&
		     p->key &&
		     ZSTR_LEN(p->key) == ZSTR_LEN(key) &&
		     memcmp(ZSTR_VAL(p->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0)) {
			if (Z_TYPE(p->val) == IS_INDIRECT) {
				zval *data = Z_INDIRECT(p->val);	//如果是间接元素，找到真实的值

				if (UNEXPECTED(Z_TYPE_P(data) == IS_UNDEF)) {	//如果已标记为IS_UNDEF 则失败
					return FAILURE;
				} else {
					if (ht->pDestructor) {
						zval tmp;
						ZVAL_COPY_VALUE(&tmp, data);
						ZVAL_UNDEF(data);	//标记为IS_UNDEF
						ht->pDestructor(&tmp);
					} else {
						ZVAL_UNDEF(data);	//直接标记
					}
					ht->u.v.flags |= HASH_FLAG_HAS_EMPTY_IND;
				}
			} else {
				//非间接元素，直接删除
				_zend_hash_del_el_ex(ht, idx, p, prev);
			}
			return SUCCESS;
		}
		prev = p;
		idx = Z_NEXT(p->val);
	}
	return FAILURE;
}

/**
 * @description: 从哈希表中删除间接元素-char字符串
 * @param HashTable* ht 待操作哈希表
 * @param char* key 待删除字符串
 * @param size_t len 字符串长度
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_str_del_ind(HashTable *ht, const char *str, size_t len)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;
	Bucket *prev = NULL;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	h = zend_inline_hash_func(str, len);	//哈希值
	nIndex = h | ht->nTableMask;	//映射表位置

	idx = HT_HASH(ht, nIndex);	//数据表位置
	while (idx != HT_INVALID_IDX) {
		p = HT_HASH_TO_BUCKET(ht, idx);	//找到数据
		if ((p->h == h)
			 && p->key
			 && (ZSTR_LEN(p->key) == len)
			 && !memcmp(ZSTR_VAL(p->key), str, len)) {
			if (Z_TYPE(p->val) == IS_INDIRECT) {	//间接元素找到实际数据
				zval *data = Z_INDIRECT(p->val);

				if (UNEXPECTED(Z_TYPE_P(data) == IS_UNDEF)) {
					return FAILURE;
				} else {
					if (ht->pDestructor) {
						ht->pDestructor(data);
					}
					ZVAL_UNDEF(data);	//对比成功之后标记删除
					ht->u.v.flags |= HASH_FLAG_HAS_EMPTY_IND;
				}
			} else {
				//非间接元素直接删除
				_zend_hash_del_el_ex(ht, idx, p, prev);
			}
			return SUCCESS;
		}
		prev = p;
		idx = Z_NEXT(p->val);
	}
	return FAILURE;
}

/**
 * @description: 从哈希表中删除元素-char字符串
 * @param HashTable* ht 待操作哈希表
 * @param char* key 待删除字符串
 * @param size_t len 字符串长度
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_str_del(HashTable *ht, const char *str, size_t len)
{
	zend_ulong h;
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;
	Bucket *prev = NULL;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	h = zend_inline_hash_func(str, len);	//哈希值
	nIndex = h | ht->nTableMask;

	idx = HT_HASH(ht, nIndex);	//数据表索引
	while (idx != HT_INVALID_IDX) {
		p = HT_HASH_TO_BUCKET(ht, idx);	//数据
		if ((p->h == h)
			 && p->key
			 && (ZSTR_LEN(p->key) == len)
			 && !memcmp(ZSTR_VAL(p->key), str, len)) {
			_zend_hash_del_el_ex(ht, idx, p, prev);	//对比成功执行删除
			return SUCCESS;
		}
		prev = p;
		idx = Z_NEXT(p->val);
	}
	return FAILURE;
}


/**
 * @description: 从哈希表中通过索引删除元素
 * @param HashTable* ht 待操作哈希表
 * @param zend_ulong h 待删除的索引
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_index_del(HashTable *ht, zend_ulong h)
{
	uint32_t nIndex;
	uint32_t idx;
	Bucket *p;
	Bucket *prev = NULL;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (ht->u.flags & HASH_FLAG_PACKED) {	//数值索引哈希表
		if (h < ht->nNumUsed) {
			p = ht->arData + h;
			if (Z_TYPE(p->val) != IS_UNDEF) {
				_zend_hash_del_el_ex(ht, HT_IDX_TO_HASH(h), p, NULL);	//找到索引的数据执行删除
				return SUCCESS;
			}
		}
		return FAILURE;
	}

	//枚举索引的哈希表
	nIndex = h | ht->nTableMask; //计算映射表位置

	idx = HT_HASH(ht, nIndex);	//数据表位置
	while (idx != HT_INVALID_IDX) {
		p = HT_HASH_TO_BUCKET(ht, idx);	//找到要删除的数据
		if ((p->h == h) && (p->key == NULL)) {
			_zend_hash_del_el_ex(ht, idx, p, prev);	//执行删除
			return SUCCESS;
		}
		prev = p;
		idx = Z_NEXT(p->val);
	}
	return FAILURE;
}

/**
 * @description: 销毁哈希表
 * @param HashTable* ht 待操作的哈希表
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_destroy(HashTable *ht)
{
	Bucket *p, *end;

	//检测状态
	IS_CONSISTENT(ht);
	HT_ASSERT(ht, GC_REFCOUNT(ht) <= 1);

	if (ht->nNumUsed) {
		p = ht->arData;
		end = p + ht->nNumUsed;

		//如果设置了哈希表析构方法
		if (ht->pDestructor) { 
			SET_INCONSISTENT(HT_IS_DESTROYING); //设置哈希表状态为正在销毁

			if (HT_HAS_STATIC_KEYS_ONLY(ht)) {
				if (HT_IS_WITHOUT_HOLES(ht)) {	//没有标记IS_UNDEF元素，逐个调用析构函数销毁
					do {
						ht->pDestructor(&p->val);
					} while (++p != end);
				} else {
					//有标记IS_UNDEF的元素，调用析构函数释放未标记的元素
					do {
						if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {
							ht->pDestructor(&p->val);
						}
					} while (++p != end);
				}
			} else if (HT_IS_WITHOUT_HOLES(ht)) {
				//没有标记为IS_UNDEF的数据，非静态key的哈希表，逐个调用析构函数释放，并释放key的zend_string的内存
				do {
					ht->pDestructor(&p->val);
					if (EXPECTED(p->key)) {
						zend_string_release(p->key);
					}
				} while (++p != end);
			} else {
				//释放为标记IS_UNDEF的数据内存
				do {
					if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {
						ht->pDestructor(&p->val);
						if (EXPECTED(p->key)) {
							zend_string_release(p->key);
						}
					}
				} while (++p != end);
			}

			SET_INCONSISTENT(HT_DESTROYED); //设置状态为销毁完成
		} else {
			if (!HT_HAS_STATIC_KEYS_ONLY(ht)) {
				do {
					if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {
						if (EXPECTED(p->key)) {
							zend_string_release(p->key);	//销毁key
						}
					}
				} while (++p != end);
			}
		}
		zend_hash_iterators_remove(ht);	//从迭代其中删除
	} else if (EXPECTED(!(ht->u.flags & HASH_FLAG_INITIALIZED))) {
		return;
	}
	pefree(HT_GET_DATA_ADDR(ht), ht->u.flags & HASH_FLAG_PERSISTENT); //释放哈希表内存
}

/**
 * @description: 销毁数组
 * @param HashTable 待操作的数组
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_array_destroy(HashTable *ht)
{
	Bucket *p, *end;

	IS_CONSISTENT(ht);
	HT_ASSERT(ht, GC_REFCOUNT(ht) <= 1);

	/* break possible cycles */
	GC_REMOVE_FROM_BUFFER(ht);	//从GC池中移除
	GC_TYPE_INFO(ht) = IS_NULL | (GC_WHITE << 16);

	if (ht->nNumUsed) {
		/* In some rare cases destructors of regular arrays may be changed */
		if (UNEXPECTED(ht->pDestructor != ZVAL_PTR_DTOR)) {
			zend_hash_destroy(ht);	//如果析构函数被修改了，则销毁调用哈希表释放函数释放数组，并且释放所用内存
			goto free_ht;
		}

		p = ht->arData;
		end = p + ht->nNumUsed;
		SET_INCONSISTENT(HT_IS_DESTROYING);	//设置状态为正在销毁

		if (HT_HAS_STATIC_KEYS_ONLY(ht)) { //静态key或者数值索引数组
			do {
				i_zval_ptr_dtor(&p->val ZEND_FILE_LINE_CC);
			} while (++p != end);
		} else if (HT_IS_WITHOUT_HOLES(ht)) {	//所有元素都有效
			do {
				i_zval_ptr_dtor(&p->val ZEND_FILE_LINE_CC);
				if (EXPECTED(p->key)) {
					zend_string_release(p->key);
				}
			} while (++p != end);
		} else {
			do {
				if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {	//释放未标记IS_UNDEF的元素
					i_zval_ptr_dtor(&p->val ZEND_FILE_LINE_CC);
					if (EXPECTED(p->key)) {
						zend_string_release(p->key);
					}
				}
			} while (++p != end);
		}
		zend_hash_iterators_remove(ht);	//从全局迭代器中移除
		SET_INCONSISTENT(HT_DESTROYED);	//设置状态为已销毁
	} else if (EXPECTED(!(ht->u.flags & HASH_FLAG_INITIALIZED))) {	//未初始化数组直接释放
		goto free_ht;
	}
	efree(HT_GET_DATA_ADDR(ht));
free_ht:
	FREE_HASHTABLE(ht);
}

/**
 * @description: 哈希表清理
 * @param HashTable* ht
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_clean(HashTable *ht)
{
	Bucket *p, *end;

	//检测状态和GC
	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (ht->nNumUsed) {
		p = ht->arData;
		end = p + ht->nNumUsed;
		if (ht->pDestructor) { //如果已绑定析构函数
			if (HT_HAS_STATIC_KEYS_ONLY(ht)) {
				if (HT_IS_WITHOUT_HOLES(ht)) { //没有IS_UNDEF的元素
					do {
						ht->pDestructor(&p->val);
					} while (++p != end);
				} else {
					do {
						if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) { //清理非IS_UNDEF元素
							ht->pDestructor(&p->val);
						}
					} while (++p != end);
				}
			} else if (HT_IS_WITHOUT_HOLES(ht)) { //所有元素都有效
				do {
					ht->pDestructor(&p->val);
					if (EXPECTED(p->key)) {
						zend_string_release(p->key);
					}
				} while (++p != end);
			} else {
				do { //只清理有效元素
					if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {
						ht->pDestructor(&p->val);
						if (EXPECTED(p->key)) {
							zend_string_release(p->key);
						}
					}
				} while (++p != end);
			}
		} else {	//未绑定析构函数
			if (!HT_HAS_STATIC_KEYS_ONLY(ht)) {
				if (HT_IS_WITHOUT_HOLES(ht)) {	//所有元素都有效
					do {
						if (EXPECTED(p->key)) {
							zend_string_release(p->key);
						}
					} while (++p != end);
				} else {
					do {	//清理有效元素
						if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {
							if (EXPECTED(p->key)) {
								zend_string_release(p->key);
							}
						}
					} while (++p != end);
				}
			}
		}
		if (!(ht->u.flags & HASH_FLAG_PACKED)) {
			HT_HASH_RESET(ht);	//非数值索引数组，清理索引区
		}
	}

	//初始化各个属性
	ht->nNumUsed = 0;	//总元素数
	ht->nNumOfElements = 0;	//有效元素数
	ht->nNextFreeElement = 0;	//下一个插入元素的数值索引
	ht->nInternalPointer = HT_INVALID_IDX;	//内部索引指针
}

/**
 * @description: 清理符号哈希表
 * @param HashTable* ht 待操作哈希表
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_symtable_clean(HashTable *ht)
{
	Bucket *p, *end;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (ht->nNumUsed) {
		p = ht->arData;
		end = p + ht->nNumUsed;
		if (HT_HAS_STATIC_KEYS_ONLY(ht)) {
			do {
				i_zval_ptr_dtor(&p->val ZEND_FILE_LINE_CC);
			} while (++p != end);
		} else if (HT_IS_WITHOUT_HOLES(ht)) {	//全部元素有效
			do {
				i_zval_ptr_dtor(&p->val ZEND_FILE_LINE_CC);
				if (EXPECTED(p->key)) {
					zend_string_release(p->key);
				}
			} while (++p != end);
		} else {
			do { //清理有效元素
				if (EXPECTED(Z_TYPE(p->val) != IS_UNDEF)) {
					i_zval_ptr_dtor(&p->val ZEND_FILE_LINE_CC);
					if (EXPECTED(p->key)) {
						zend_string_release(p->key);
					}
				}
			} while (++p != end);
		}
		HT_HASH_RESET(ht);	//重置索引区
	}

	//重置属性值
	ht->nNumUsed = 0;
	ht->nNumOfElements = 0;
	ht->nNextFreeElement = 0;
	ht->nInternalPointer = HT_INVALID_IDX;
}

/**
 * @description: 优雅的销毁哈希表
 * @param HashTable* ht 待操作的哈希表
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_graceful_destroy(HashTable *ht)
{
	uint32_t idx;
	Bucket *p;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	p = ht->arData;
	for (idx = 0; idx < ht->nNumUsed; idx++, p++) {
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;	//如果该元素已标记为IS_UNDEF则跳过
		_zend_hash_del_el(ht, HT_IDX_TO_HASH(idx), p);	//删除元素
	}
	if (ht->u.flags & HASH_FLAG_INITIALIZED) {
		pefree(HT_GET_DATA_ADDR(ht), ht->u.flags & HASH_FLAG_PERSISTENT);	//释放哈希表内存
	}

	SET_INCONSISTENT(HT_DESTROYED);	//设置哈希表状态为已销毁
}

/**
 * @description: 优雅的逆向销毁哈希表
 * @param HashTable* ht 待操作的哈希表
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_graceful_reverse_destroy(HashTable *ht)
{
	uint32_t idx;
	Bucket *p;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	idx = ht->nNumUsed;
	p = ht->arData + ht->nNumUsed;	//从尾部开始逐个销毁非IS_UNDEF元素
	while (idx > 0) {
		idx--;
		p--;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;
		_zend_hash_del_el(ht, HT_IDX_TO_HASH(idx), p);
	}

	if (ht->u.flags & HASH_FLAG_INITIALIZED) {
		pefree(HT_GET_DATA_ADDR(ht), ht->u.flags & HASH_FLAG_PERSISTENT);	//释放内存
	}

	SET_INCONSISTENT(HT_DESTROYED);	//设置状态为已销毁
}

/* This is used to recurse elements and selectively delete certain entries
 * from a hashtable. apply_func() receives the data and decides if the entry
 * should be deleted or recursion should be stopped. The following three
 * return codes are possible:
 * ZEND_HASH_APPLY_KEEP   - continue
 * ZEND_HASH_APPLY_STOP   - stop iteration
 * ZEND_HASH_APPLY_REMOVE - delete the element, combineable with the former
 * 
 * @description: 对哈希表使用带有参数的回调函数，通过函数的返回值判断是否删除元素，【递归保护】
 * @param HashTable* ht 待操作的哈希表
 * @param apply_func_arg_t apply_func 回调函数
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_apply(HashTable *ht, apply_func_t apply_func)
{
	uint32_t idx;
	Bucket *p;
	int result;

	IS_CONSISTENT(ht);

	HASH_PROTECT_RECURSION(ht); //递归保护以及深度检测
	for (idx = 0; idx < ht->nNumUsed; idx++) {
		p = ht->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;
		result = apply_func(&p->val);

		if (result & ZEND_HASH_APPLY_REMOVE) {
			HT_ASSERT_RC1(ht);
			_zend_hash_del_el(ht, HT_IDX_TO_HASH(idx), p);
		}
		if (result & ZEND_HASH_APPLY_STOP) {
			break;
		}
	}
	HASH_UNPROTECT_RECURSION(ht);
}

/**
 * @description: 对哈希表使用带有一个参数的回调函数，通过函数的返回值判断是否删除元素，【递归保护】
 * @param HashTable* ht 待操作的哈希表
 * @param apply_func_arg_t apply_func 回调函数
 * @param void* argument回调函数的参数
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_apply_with_argument(HashTable *ht, apply_func_arg_t apply_func, void *argument)
{
    uint32_t idx;
	Bucket *p;
	int result;

	IS_CONSISTENT(ht);

	HASH_PROTECT_RECURSION(ht);	//递归保护以及深度检测
	for (idx = 0; idx < ht->nNumUsed; idx++) {
		p = ht->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue; //跳过IS_UNDEF的元素
		result = apply_func(&p->val, argument);	//对元素执行回调函数，

		if (result & ZEND_HASH_APPLY_REMOVE) {	//如果回调函数标记了移除属性，则删除元素
			HT_ASSERT_RC1(ht);
			_zend_hash_del_el(ht, HT_IDX_TO_HASH(idx), p);
		}
		if (result & ZEND_HASH_APPLY_STOP) {	//如果回调函数标记了停止操作，则停止删除
			break;
		}
	}
	HASH_UNPROTECT_RECURSION(ht);
}

/**
 * @description: 对哈希表使用带有多个参数的回调函数，通过函数的返回值判断是否删除元素，【递归保护】
 * @param HashTable* ht 待操作的哈希表
 * @param apply_func_arg_t apply_func 回调函数
 * @param int num_args 回调函数参数个数
 * @param va_list 回调函数的参数列表
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_apply_with_arguments(HashTable *ht, apply_func_args_t apply_func, int num_args, ...)
{
	uint32_t idx;
	Bucket *p;
	va_list args;
	zend_hash_key hash_key;
	int result;

	IS_CONSISTENT(ht);

	HASH_PROTECT_RECURSION(ht);

	for (idx = 0; idx < ht->nNumUsed; idx++) {
		p = ht->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue; //跳过IS_UNDEF元素
		va_start(args, num_args);
		hash_key.h = p->h;
		hash_key.key = p->key;

		result = apply_func(&p->val, num_args, args, &hash_key);	//对元素调用多参数回调函数

		if (result & ZEND_HASH_APPLY_REMOVE) {
			HT_ASSERT_RC1(ht);
			_zend_hash_del_el(ht, HT_IDX_TO_HASH(idx), p);	//执行删除
		}
		if (result & ZEND_HASH_APPLY_STOP) {	//停止删除
			va_end(args);
			break;
		}
		va_end(args);
	}

	HASH_UNPROTECT_RECURSION(ht);
}

 /* 
 * @description: 对哈希表逆向使用带有参数的回调函数，通过函数的返回值判断是否删除元素，【递归保护】
 * @param HashTable* ht 待操作的哈希表
 * @param apply_func_arg_t apply_func 回调函数
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_reverse_apply(HashTable *ht, apply_func_t apply_func)
{
	uint32_t idx;
	Bucket *p;
	int result;

	IS_CONSISTENT(ht);

	HASH_PROTECT_RECURSION(ht);
	idx = ht->nNumUsed;
	while (idx > 0) {
		idx--;
		p = ht->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;

		result = apply_func(&p->val);

		if (result & ZEND_HASH_APPLY_REMOVE) {
			HT_ASSERT_RC1(ht);
			_zend_hash_del_el(ht, HT_IDX_TO_HASH(idx), p);
		}
		if (result & ZEND_HASH_APPLY_STOP) {
			break;
		}
	}
	HASH_UNPROTECT_RECURSION(ht);
}

/**
 * @description: 复制哈希表中的有效元素到另一个哈希表
 * @param HashTable* target 目标哈希表
 * @param HashTable* source 原哈希表
 * @param copy_ctor_func_t pCopyConstructor 回调函数
 * @return: 
 */
ZEND_API void ZEND_FASTCALL zend_hash_copy(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor)
{
    uint32_t idx;
	Bucket *p;
	zval *new_entry, *data;
	zend_bool setTargetPointer;

	IS_CONSISTENT(source);
	IS_CONSISTENT(target);
	HT_ASSERT_RC1(target);

	setTargetPointer = (target->nInternalPointer == HT_INVALID_IDX);
	for (idx = 0; idx < source->nNumUsed; idx++) {
		p = source->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;	//跳过无效元素

		if (setTargetPointer && source->nInternalPointer == idx) {	//设置内部指针位置
			target->nInternalPointer = HT_INVALID_IDX;
		}
		/* INDIRECT element may point to UNDEF-ined slots */
		data = &p->val;
		if (Z_TYPE_P(data) == IS_INDIRECT) {	//如果元素是间接类型
			data = Z_INDIRECT_P(data);
			if (UNEXPECTED(Z_TYPE_P(data) == IS_UNDEF)) {	//标记实际值为IS_UNDEF
				continue;
			}
		}
		if (p->key) {
			new_entry = zend_hash_update(target, p->key, data); //更新字符索引元素
		} else {
			new_entry = zend_hash_index_update(target, p->h, data);	//更新数值索引元素
		}
		if (pCopyConstructor) {
			pCopyConstructor(new_entry);
		}
	}

	//如果内部指针为无效，并且有效元素大于0，则将第一个有效元素索引赋值给内部指针位
	if (target->nInternalPointer == HT_INVALID_IDX && target->nNumOfElements > 0) {	
		idx = 0;
		while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
			idx++;
		}
		target->nInternalPointer = idx;
	}
}

/**
 * @description: 复制一个数组的元素到另一个数组的元素上
 * @param HashTable* source 原哈希表 
 * @param HashTable* target 目标哈希表 
 * @param uint32_t idx 目标哈希表索引位值
 * @param Bucket* p 将要复制元素
 * @param Bucket* q 复制到目标元素 p ---> q
 * @param int packed 是否数值索引
 * @param int with_holes 原元素是否有效
 * @return: 
 */
static zend_always_inline int zend_array_dup_element(HashTable *source, HashTable *target, uint32_t idx, Bucket *p, Bucket *q, int packed, int static_keys, int with_holes)
{
	zval *data = &p->val;

	if (with_holes) {
		if (!packed && Z_TYPE_INFO_P(data) == IS_INDIRECT) {	//非间接类型并且非数字索引，取其指向的实际数据
			data = Z_INDIRECT_P(data);
		}
		if (UNEXPECTED(Z_TYPE_INFO_P(data) == IS_UNDEF)) {	//如果数据已标记为IS_UNDEF则返回
			return 0;
		}
	} else if (!packed) {
		/* INDIRECT element may point to UNDEF-ined slots */
		if (Z_TYPE_INFO_P(data) == IS_INDIRECT) {
			data = Z_INDIRECT_P(data);
			if (UNEXPECTED(Z_TYPE_INFO_P(data) == IS_UNDEF)) {
				return 0;
			}
		}
	}

	do {
		if (Z_OPT_REFCOUNTED_P(data)) {	//如果是引用类型的数据
			if (Z_ISREF_P(data) && Z_REFCOUNT_P(data) == 1 &&
			    (Z_TYPE_P(Z_REFVAL_P(data)) != IS_ARRAY ||
			      Z_ARRVAL_P(Z_REFVAL_P(data)) != source)) {
				data = Z_REFVAL_P(data);	//返回引用的val
				if (!Z_OPT_REFCOUNTED_P(data)) {	//如果引用的val非引用计数类型，跳过。否则引用计数加+1
					break;
				}
			}
			Z_ADDREF_P(data);
		}
	} while (0);
	ZVAL_COPY_VALUE(&q->val, data); //将data拷贝到q->val上，此处只是将data的gc信息和类型信息赋值给q->val

	q->h = p->h;
	if (packed) {
		q->key = NULL;	//如果是数值索引的哈希表，将key设为NULL
	} else {
		uint32_t nIndex;

		q->key = p->key;	//枚举索引哈希表，
		if (!static_keys && q->key) {	//非静态key并且key不为空，key的引用计数+1
			zend_string_addref(q->key);
		}

		nIndex = q->h | target->nTableMask;	//计算索引映射表位置（solt）
		Z_NEXT(q->val) = HT_HASH(target, nIndex);	//将映射表的原值赋值给q->val的u2.next
		HT_HASH(target, nIndex) = HT_IDX_TO_HASH(idx);	//将idx赋值到映射表
	}
	return 1;
}

/**
 * @description: 复制数值索引数组到另一个数组
 * @param HashTable* source 原哈希表 
 * @param HashTable* target 目标哈希表 
 * @param int with_holes 是否要标记IS_UNDEF
 * @return: 
 */
static zend_always_inline void zend_array_dup_packed_elements(HashTable *source, HashTable *target, int with_holes)
{
	Bucket *p = source->arData;
	Bucket *q = target->arData;
	Bucket *end = p + source->nNumUsed;

	do {
		if (!zend_array_dup_element(source, target, 0, p, q, 1, 1, with_holes)) {	//逐个复制原数组的元素到目标数组上
			if (with_holes) {	//如果要标记为IS_UNDEF
				ZVAL_UNDEF(&q->val);
			}
		}
		p++; q++;
	} while (p != end);
}

/**
 * @description: 复制数值静态key数组到另一个数组
 * @param HashTable* source 原哈希表 
 * @param HashTable* target 目标哈希表 
 * @param int static_keys 是否key
 * @param int with_holes 是否要标记IS_UNDEF
 * @return: 
 */
static zend_always_inline uint32_t zend_array_dup_elements(HashTable *source, HashTable *target, int static_keys, int with_holes)
{
	uint32_t idx = 0;
	Bucket *p = source->arData;
	Bucket *q = target->arData;
	Bucket *end = p + source->nNumUsed;

	do {
		if (!zend_array_dup_element(source, target, idx, p, q, 0, static_keys, with_holes)) {
			uint32_t target_idx = idx;

			idx++; p++;
			while (p != end) {
				if (zend_array_dup_element(source, target, target_idx, p, q, 0, static_keys, with_holes)) {
					if (source->nInternalPointer == idx) {
						target->nInternalPointer = target_idx;
					}
					target_idx++; q++;
				}
				idx++; p++;
			}
			return target_idx;
		}
		idx++; p++; q++;
	} while (p != end);
	return idx;
}

/**
 * @description: 复制一个数组
 * @param HashTable* source 源数组
 * @return: HashTable* 复制完成的数组
 */
ZEND_API HashTable* ZEND_FASTCALL zend_array_dup(HashTable *source)
{
	uint32_t idx;
	HashTable *target;

	IS_CONSISTENT(source);

	ALLOC_HASHTABLE(target);	//申请新数组内存
	GC_REFCOUNT(target) = 1;	//设置新数组gc
	GC_TYPE_INFO(target) = IS_ARRAY | (GC_COLLECTABLE << GC_FLAGS_SHIFT);	//设置新数组属性

	target->nTableSize = source->nTableSize;	//新数组大小与原数组一致
	target->pDestructor = ZVAL_PTR_DTOR;	//设置新数组析构函数

	if (source->nNumUsed == 0) {	//如果原数组已用元素为0，则将新数组的各属性设置为初始状态
		target->u.flags = (source->u.flags & ~(HASH_FLAG_INITIALIZED|HASH_FLAG_PACKED|HASH_FLAG_PERSISTENT|ZEND_HASH_APPLY_COUNT_MASK)) | HASH_FLAG_APPLY_PROTECTION | HASH_FLAG_STATIC_KEYS;
		target->nTableMask = HT_MIN_MASK;
		target->nNumUsed = 0;
		target->nNumOfElements = 0;
		target->nNextFreeElement = 0;
		target->nInternalPointer = HT_INVALID_IDX;
		HT_SET_DATA_ADDR(target, &uninitialized_bucket);
	} else if (GC_FLAGS(source) & IS_ARRAY_IMMUTABLE) { 
		//如果原数组是不可变数组，将原数组的各个属性复制给新数组上
		target->u.flags = (source->u.flags & ~HASH_FLAG_PERSISTENT) | HASH_FLAG_APPLY_PROTECTION;	//设置新数组属性为原数组去掉永久属性，增加递归保护属性
		target->nTableMask = source->nTableMask;
		target->nNumUsed = source->nNumUsed;
		target->nNumOfElements = source->nNumOfElements;
		target->nNextFreeElement = source->nNextFreeElement;
		HT_SET_DATA_ADDR(target, emalloc(HT_SIZE(target)));
		target->nInternalPointer = source->nInternalPointer;
		memcpy(HT_GET_DATA_ADDR(target), HT_GET_DATA_ADDR(source), HT_USED_SIZE(source));
		//如果新数组有效元素大于0，并且内部指针指向无效索引，将内部指针指向第一个有效索引
		if (target->nNumOfElements > 0 &&
		    target->nInternalPointer == HT_INVALID_IDX) {
			idx = 0;
			while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
				idx++;
			}
			target->nInternalPointer = idx;
		}
	} else if (source->u.flags & HASH_FLAG_PACKED) {
		//如果原数组是数值索引数组，
		target->u.flags = (source->u.flags & ~(HASH_FLAG_PERSISTENT|ZEND_HASH_APPLY_COUNT_MASK)) | HASH_FLAG_APPLY_PROTECTION;
		target->nTableMask = source->nTableMask;
		target->nNumUsed = source->nNumUsed;
		target->nNumOfElements = source->nNumOfElements;
		target->nNextFreeElement = source->nNextFreeElement;
		HT_SET_DATA_ADDR(target, emalloc(HT_SIZE(target)));	//新数组的arData指针指向第一个
		target->nInternalPointer = source->nInternalPointer;
		HT_HASH_RESET_PACKED(target);	//重置映射表

		//复制元素到新数组
		if (HT_IS_WITHOUT_HOLES(target)) {
			zend_array_dup_packed_elements(source, target, 0); 
		} else {
			zend_array_dup_packed_elements(source, target, 1);
		}

		//设置内部指针到第一个有效数据
		if (target->nNumOfElements > 0 &&
		    target->nInternalPointer == HT_INVALID_IDX) {
			idx = 0;
			while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
				idx++;
			}
			target->nInternalPointer = idx;
		}
	} else {
		//字符索引数组处理
		target->u.flags = (source->u.flags & ~(HASH_FLAG_PERSISTENT|ZEND_HASH_APPLY_COUNT_MASK)) | HASH_FLAG_APPLY_PROTECTION;
		target->nTableMask = source->nTableMask;
		target->nNextFreeElement = source->nNextFreeElement;
		target->nInternalPointer = source->nInternalPointer;

		HT_SET_DATA_ADDR(target, emalloc(HT_SIZE(target)));	//移动arData指针到第一个数据
		HT_HASH_RESET(target);	//重置映射表

		if (HT_HAS_STATIC_KEYS_ONLY(target)) {
			//复制静态数组
			if (HT_IS_WITHOUT_HOLES(source)) {
				idx = zend_array_dup_elements(source, target, 1, 0);
			} else {
				idx = zend_array_dup_elements(source, target, 1, 1);
			}
		} else {
			//复制非静态数组
			if (HT_IS_WITHOUT_HOLES(source)) {
				idx = zend_array_dup_elements(source, target, 0, 0);
			} else {
				idx = zend_array_dup_elements(source, target, 0, 1);
			}
		}
		target->nNumUsed = idx;
		target->nNumOfElements = idx;

		//内部指针指向第一个元素
		if (idx > 0 && target->nInternalPointer == HT_INVALID_IDX) {
			target->nInternalPointer = 0;
		}
	}

	//返回新数组
	return target;
}

/**
 * @description: 合并两个哈希表
 * @param HashTable* target 目标哈希表（主）
 * @param HashTable* source 源哈希表（副）
 * @param copy_ctor_func_t pCopyConstructor 复制回调函数
 * @param zend_bool overwrite 是否覆盖
 * @return: 
 */
ZEND_API void ZEND_FASTCALL _zend_hash_merge(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, zend_bool overwrite ZEND_FILE_LINE_DC)
{
    uint32_t idx;
	Bucket *p;
	zval *t;

	IS_CONSISTENT(source);	//检测源哈希表状态
	IS_CONSISTENT(target);	//检测目标哈希表状态
	HT_ASSERT_RC1(target);	//检测目标哈希表gc

	if (overwrite) {
		//覆盖合并
		for (idx = 0; idx < source->nNumUsed; idx++) {
			p = source->arData + idx;
			if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue; //跳过无效数据
			if (UNEXPECTED(Z_TYPE(p->val) == IS_INDIRECT) &&
			    UNEXPECTED(Z_TYPE_P(Z_INDIRECT(p->val)) == IS_UNDEF)) {
			    continue;	//间接数据无效也跳过
			}

			if (p->key) {
				//更新字符索引的元素到哈希表
				t = _zend_hash_add_or_update_i(target, p->key, &p->val, HASH_UPDATE | HASH_UPDATE_INDIRECT ZEND_FILE_LINE_RELAY_CC);
				if (t && pCopyConstructor) {
					pCopyConstructor(t);
				}
			} else {
				//更新数值索引的元素到哈希表
				t = zend_hash_index_update(target, p->h, &p->val);
				if (t && pCopyConstructor) {
					pCopyConstructor(t);
				}
			}
		}
	} else {
		//不覆盖合并
		for (idx = 0; idx < source->nNumUsed; idx++) {
			p = source->arData + idx;
			if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;	//跳过无效元素
			if (UNEXPECTED(Z_TYPE(p->val) == IS_INDIRECT) &&
			    UNEXPECTED(Z_TYPE_P(Z_INDIRECT(p->val)) == IS_UNDEF)) {
			    continue;	//跳过间接值是无效的元素
			}
			if (p->key) {
				//添加字符索引元素到哈希表
				t = _zend_hash_add_or_update_i(target, p->key, &p->val, HASH_ADD | HASH_UPDATE_INDIRECT ZEND_FILE_LINE_RELAY_CC);
				if (t && pCopyConstructor) {
					pCopyConstructor(t);
				}
			} else {
				//添加数值索引元素到哈希表
				t = zend_hash_index_add(target, p->h, &p->val);
				if (t && pCopyConstructor) {
					pCopyConstructor(t);
				}
			}
		}
	}

	//如果目标哈希表的有效元素个数大于0，则将内部指针移动到第一个有效元素上
	if (target->nNumOfElements > 0) {
		idx = 0;
		while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
			idx++;
		}
		target->nInternalPointer = idx;
	}
}

/**
 * @description: 对哈希表元素执行替换检测
 * @param HashTable* target 目标哈希表
 * @param zval* source_data 原数据
 * @param Bucket* p 元素
 * @param void* pParam 参数
 * @param merge_checker_func_t merge_checker_func 实际检测函数
 * @return: zend_bool 是否可以替换
 */
static zend_bool ZEND_FASTCALL zend_hash_replace_checker_wrapper(HashTable *target, zval *source_data, Bucket *p, void *pParam, merge_checker_func_t merge_checker_func)
{
	zend_hash_key hash_key;

	hash_key.h = p->h;
	hash_key.key = p->key;
	return merge_checker_func(target, source_data, &hash_key, pParam);
}

/**
 * @description: 哈希表合并
 * @param HashTable* target 目标哈希表
 * @param HashTable* 源哈希表
 * @param copy_ctor_func_t  pCopyConstructor 复制回调函数
 * @param merge_checker_func_t pMergeSource 合并检测函数
 * @param void* pParam 参数
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_merge_ex(HashTable *target, HashTable *source, copy_ctor_func_t pCopyConstructor, merge_checker_func_t pMergeSource, void *pParam)
{
	uint32_t idx;
	Bucket *p;
	zval *t;

	IS_CONSISTENT(source);
	IS_CONSISTENT(target);
	HT_ASSERT_RC1(target);

	for (idx = 0; idx < source->nNumUsed; idx++) {
		p = source->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue; //跳过无效元素
		if (zend_hash_replace_checker_wrapper(target, &p->val, p, pParam, pMergeSource)) {
			t = zend_hash_update(target, p->key, &p->val);	//更新元素到目标哈希表
			if (t && pCopyConstructor) {
				pCopyConstructor(t);
			}
		}
	}
	//移动内部指针到第一个有效元素
	if (target->nNumOfElements > 0) {
		idx = 0;
		while (Z_TYPE(target->arData[idx].val) == IS_UNDEF) {
			idx++;
		}
		target->nInternalPointer = idx;
	}
}


/* Returns the hash table data if found and NULL if not. */
/**
 * @description: 通过zend_string类型的key从哈希表查找元素
 * @param HashTable* ht 待查哈希表
 * @param zend_string* key 待查找key
 * @return: zval* | NULL
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_find(const HashTable *ht, zend_string *key)
{
	Bucket *p;

	IS_CONSISTENT(ht);

	p = zend_hash_find_bucket(ht, key);
	return p ? &p->val : NULL;
}

/**
 * @description: 通过字符串key从哈希表中查找元素
 * @param HashTable* ht 待查哈希表
 * @param char* str 待查找key
 * @param size_t len key长度
 * @return: zval* | NULL
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_str_find(const HashTable *ht, const char *str, size_t len)
{
	zend_ulong h;
	Bucket *p;

	IS_CONSISTENT(ht);

	h = zend_inline_hash_func(str, len);
	p = zend_hash_str_find_bucket(ht, str, len, h);
	return p ? &p->val : NULL;
}

/**
 * @description: 通过zend_string key判断是否存在哈希表中
 * @param HashTable* ht 待查哈希表
 * @param zend_string* key 待查找key
 * @return: zend_bool 是否存在
 */
ZEND_API zend_bool ZEND_FASTCALL zend_hash_exists(const HashTable *ht, zend_string *key)
{
	Bucket *p;

	IS_CONSISTENT(ht);

	p = zend_hash_find_bucket(ht, key);
	return p ? 1 : 0;
}

/**
 * @description: 通过zend_string key判断是否存在哈希表中
 * @param HashTable* ht 待查哈希表
 * @param char* str 待查找key
 * @param size_t len key长度
 * @return: zend_bool 是否存在
 */
ZEND_API zend_bool ZEND_FASTCALL zend_hash_str_exists(const HashTable *ht, const char *str, size_t len)
{
	zend_ulong h;
	Bucket *p;

	IS_CONSISTENT(ht);

	h = zend_inline_hash_func(str, len);
	p = zend_hash_str_find_bucket(ht, str, len, h);
	return p ? 1 : 0;
}

/**
 * @description: 通过哈希值或数值索引从哈希表中查找元素
 * @param HashTable* ht 待查哈希表
 * @param zend_ulong h 哈希值
 * @return: zval* | NULL 
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_index_find(const HashTable *ht, zend_ulong h)
{
	Bucket *p;

	IS_CONSISTENT(ht);

	if (ht->u.flags & HASH_FLAG_PACKED) {
		if (h < ht->nNumUsed) {
			p = ht->arData + h;
			if (Z_TYPE(p->val) != IS_UNDEF) {
				return &p->val;
			}
		}
		return NULL;
	}

	p = zend_hash_index_find_bucket(ht, h);
	return p ? &p->val : NULL;
}

/**
 * @description: 通过哈希值从哈希表中查找元素（只查找枚举哈希表）
 * @param HashTable* ht 待查哈希表
 * @param zend_ulong h 哈希值
 * @return: zval* | NULL 
 */
ZEND_API zval* ZEND_FASTCALL _zend_hash_index_find(const HashTable *ht, zend_ulong h)
{
	Bucket *p;

	IS_CONSISTENT(ht);

	p = zend_hash_index_find_bucket(ht, h);
	return p ? &p->val : NULL;
}

/**
 * @description: 判断哈希值或数值索引是否存在于哈希表
 * @param HashTable* ht 待查哈希表
 * @param zend_ulong h 哈希值
 * @return: zval* | NULL 
 */
ZEND_API zend_bool ZEND_FASTCALL zend_hash_index_exists(const HashTable *ht, zend_ulong h)
{
	Bucket *p;

	IS_CONSISTENT(ht);

	if (ht->u.flags & HASH_FLAG_PACKED) {
		if (h < ht->nNumUsed) {
			if (Z_TYPE(ht->arData[h].val) != IS_UNDEF) {
				return 1;
			}
		}
		return 0;
	}

	p = zend_hash_index_find_bucket(ht, h);
	return p ? 1 : 0;
}

/**
 * @description: 重置哈希表内部指针
 * @param {type} 
 * @return: 
 */
ZEND_API void ZEND_FASTCALL zend_hash_internal_pointer_reset_ex(HashTable *ht, HashPosition *pos)
{
    uint32_t idx;

	IS_CONSISTENT(ht);
	HT_ASSERT(ht, &ht->nInternalPointer != pos || GC_REFCOUNT(ht) == 1);

	for (idx = 0; idx < ht->nNumUsed; idx++) {
		if (Z_TYPE(ht->arData[idx].val) != IS_UNDEF) {
			*pos = idx;
			return;
		}
	}
	*pos = HT_INVALID_IDX;
}


/* This function will be extremely optimized by remembering
 * the end of the list
 */

/**
 * @description: pos 设置为哈希表最后一个有效元素坐标
 * @param HashTable* ht 待操作哈希表
 * @param HashPosition* pos 坐标
 * @return: 
 */
ZEND_API void ZEND_FASTCALL zend_hash_internal_pointer_end_ex(HashTable *ht, HashPosition *pos)
{
	uint32_t idx;

	IS_CONSISTENT(ht);
	HT_ASSERT(ht, &ht->nInternalPointer != pos || GC_REFCOUNT(ht) == 1);

	idx = ht->nNumUsed;
	while (idx > 0) {
		idx--;
		if (Z_TYPE(ht->arData[idx].val) != IS_UNDEF) {
			*pos = idx;
			return;
		}
	}
	*pos = HT_INVALID_IDX;
}

/**
 * @description: 移动pos到后面的最近一个有效元素坐标
 * @param HashTable* ht 待操作哈希表
 * @param HashPosition* pos 待操作坐标
 * @return: 
 */
ZEND_API int ZEND_FASTCALL zend_hash_move_forward_ex(HashTable *ht, HashPosition *pos)
{
	uint32_t idx = *pos;

	IS_CONSISTENT(ht);
	HT_ASSERT(ht, &ht->nInternalPointer != pos || GC_REFCOUNT(ht) == 1);

	if (idx != HT_INVALID_IDX) {
		while (1) {
			idx++;
			if (idx >= ht->nNumUsed) {
				*pos = HT_INVALID_IDX;
				return SUCCESS;
			}
			
			//后移idx，并且判断有效元素
			if (Z_TYPE(ht->arData[idx].val) != IS_UNDEF) {
				*pos = idx;
				return SUCCESS;
			}
		}
	} else {
 		return FAILURE;
	}
}

/**
 * @description: 移动pos到前面的最近一个有效元素坐标
 * @param HashTable* ht 待操作哈希表
 * @param HashPosition* pos 待操作坐标
 * @return: 
 */
ZEND_API int ZEND_FASTCALL zend_hash_move_backwards_ex(HashTable *ht, HashPosition *pos)
{
	uint32_t idx = *pos;

	IS_CONSISTENT(ht);
	HT_ASSERT(ht, &ht->nInternalPointer != pos || GC_REFCOUNT(ht) == 1);

	if (idx != HT_INVALID_IDX) {
		while (idx > 0) {
			idx--;
			//前移idx，并且判断有效元素
			if (Z_TYPE(ht->arData[idx].val) != IS_UNDEF) {
				*pos = idx;
				return SUCCESS;
			}
		}
		*pos = HT_INVALID_IDX;
 		return SUCCESS;
	} else {
 		return FAILURE;
	}
}


/* This function should be made binary safe  */
/**
 * @description: 通过索引查询哈希表，并且将参数中的str_index和key设置为元素的值
 * @param HashTable* ht 待操作哈希表
 * @param zend_string* *str_index 字符串key
 * @param zend_ulong* h 哈希值
 * @return: int 元素key的类型
 */
ZEND_API int ZEND_FASTCALL zend_hash_get_current_key_ex(const HashTable *ht, zend_string **str_index, zend_ulong *num_index, HashPosition *pos)
{
	uint32_t idx = *pos;
	Bucket *p;

	IS_CONSISTENT(ht);
	if (idx != HT_INVALID_IDX) {
		p = ht->arData + idx;
		if (p->key) {	
			//字符串key
			*str_index = p->key;
			return HASH_KEY_IS_STRING;
		} else {
			//数值key
			*num_index = p->h;
			return HASH_KEY_IS_LONG;
		}
	}
	return HASH_KEY_NON_EXISTENT;
}

/**
 * @description: 通过索引获取哈希表元素，并将其赋值给参数key
 * @param HashTable* ht 待操作哈希表
 * @param zval* key 待操作的zval
 * @return: void
 */
ZEND_API void ZEND_FASTCALL zend_hash_get_current_key_zval_ex(const HashTable *ht, zval *key, HashPosition *pos)
{
	uint32_t idx = *pos;
	Bucket *p;

	IS_CONSISTENT(ht);
	if (idx == HT_INVALID_IDX) {
		ZVAL_NULL(key);	//如果索引值无效，设置key类型为IS_NULL
	} else {
		p = ht->arData + idx;
		if (p->key) {	
			//将元素的key赋值给参数key，此参数应该只操作引用和gc
			ZVAL_STR_COPY(key, p->key);
		} else {
			//将元素的h赋值给参数key的lval
			ZVAL_LONG(key, p->h);
		}
	}
}

/**
 * @description: 根据索引获取哈希表中元素的key类型
 * @param HashTable* ht 代操作哈希表
 * @param HashPosition* pos
 * @return: int
 */
ZEND_API int ZEND_FASTCALL zend_hash_get_current_key_type_ex(HashTable *ht, HashPosition *pos)
{
    uint32_t idx = *pos;
	Bucket *p;

	IS_CONSISTENT(ht);
	if (idx != HT_INVALID_IDX) {
		p = ht->arData + idx;
		if (p->key) {
			return HASH_KEY_IS_STRING;
		} else {
			return HASH_KEY_IS_LONG;
		}
	}
	return HASH_KEY_NON_EXISTENT;
}

/**
 * @description: 根据索引获取哈希表的元素数据
 * @param HashTable* ht 代操作哈希表
 * @param HashPosition* pos
 * @return: zval*
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_get_current_data_ex(HashTable *ht, HashPosition *pos)
{
	uint32_t idx = *pos;
	Bucket *p;

	IS_CONSISTENT(ht);
	if (idx != HT_INVALID_IDX) {
		p = ht->arData + idx;
		return &p->val;
	} else {
		return NULL;
	}
}

/**
 * @description: 交换哈希表中的两个元素
 * @param Bucket* p 待操作元素p
 * @param Bucket* q 待操作元素q
 * @return: void
 */
ZEND_API void zend_hash_bucket_swap(Bucket *p, Bucket *q)
{
	zval val;
	zend_ulong h;
	zend_string *key;

	//将p赋值给临变量
	ZVAL_COPY_VALUE(&val, &p->val);
	h = p->h;
	key = p->key;

	//将q赋值给p
	ZVAL_COPY_VALUE(&p->val, &q->val);
	p->h = q->h;
	p->key = q->key;

	//将临时变量赋值给q
	ZVAL_COPY_VALUE(&q->val, &val);
	q->h = h;
	q->key = key;
}

/**
 * @description: 交换哈希表中的两个元素的实际值，也相当于交换了两个元素的索引
 * @param Bucket* p 待操作元素p
 * @param Bucket* q 待操作元素q
 * @return: void
 */
ZEND_API void zend_hash_bucket_renum_swap(Bucket *p, Bucket *q)
{
	zval val;

	ZVAL_COPY_VALUE(&val, &p->val);
	ZVAL_COPY_VALUE(&p->val, &q->val);
	ZVAL_COPY_VALUE(&q->val, &val);
}

/**
 * @description: 交换数值索引哈希表中的两个元素
 * @param Bucket* p 待操作元素p
 * @param Bucket* q 待操作元素q
 * @return: void
 */
ZEND_API void zend_hash_bucket_packed_swap(Bucket *p, Bucket *q)
{
	zval val;
	zend_ulong h;

	ZVAL_COPY_VALUE(&val, &p->val);
	h = p->h;

	ZVAL_COPY_VALUE(&p->val, &q->val);
	p->h = q->h;

	ZVAL_COPY_VALUE(&q->val, &val);
	q->h = h;
}

/**
 * @description: 哈希表排序
 * @param HashTable* ht 待操作哈希表
 * @param sort_func_t 排序函数
 * @param compare_func_t 对比函数
 * @param zend_bool renumber 是否重新索引
 * @return: 
 */
ZEND_API int ZEND_FASTCALL zend_hash_sort_ex(HashTable *ht, sort_func_t sort, compare_func_t compar, zend_bool renumber)
{
	Bucket *p;
	uint32_t i, j;

	IS_CONSISTENT(ht);
	HT_ASSERT_RC1(ht);

	if (!(ht->nNumOfElements>1) && !(renumber && ht->nNumOfElements>0)) { /* Doesn't require sorting */
		return SUCCESS;
	}

	if (HT_IS_WITHOUT_HOLES(ht)) {
		i = ht->nNumUsed; //数据全部有效，i设置为元素总数量
	} else {
		//清楚掉无效数据
		for (j = 0, i = 0; j < ht->nNumUsed; j++) {
			p = ht->arData + j;
			if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;
			if (i != j) {
				ht->arData[i] = *p;
			}
			i++;
		}
	}

	sort((void *)ht->arData, i, sizeof(Bucket), compar,
			(swap_func_t)(renumber? zend_hash_bucket_renum_swap :
				((ht->u.flags & HASH_FLAG_PACKED) ? zend_hash_bucket_packed_swap : zend_hash_bucket_swap)));

	ht->nNumUsed = i;
	ht->nInternalPointer = 0;

	if (renumber) {
		//设置元素的h为数值索引，0.... h->nNumUsed
		for (j = 0; j < i; j++) {
			p = ht->arData + j;
			p->h = j;
			if (p->key) {
				zend_string_release(p->key);
				p->key = NULL;
			}
		}

		ht->nNextFreeElement = i;	//下一个插入元素的索引值
	}
	if (ht->u.flags & HASH_FLAG_PACKED) {
		//如果哈希表是数值索引，并且renumber是假，则将数值索引哈希表转换成枚举索引哈希表
		if (!renumber) {
			zend_hash_packed_to_hash(ht);
		}
	} else {
		//枚举哈希表并且设置了renumber为真，将哈希表转换成数值索引哈希表
		if (renumber) {
			void *new_data, *old_data = HT_GET_DATA_ADDR(ht);
			Bucket *old_buckets = ht->arData;

			new_data = pemalloc(HT_SIZE_EX(ht->nTableSize, HT_MIN_MASK), (ht->u.flags & HASH_FLAG_PERSISTENT));	//新申请一块内存
			ht->u.flags |= HASH_FLAG_PACKED | HASH_FLAG_STATIC_KEYS;	//设置哈希表属性为数值索引和静态keys
			ht->nTableMask = HT_MIN_MASK;	//设置表掩码为最小，数值索引需要
			HT_SET_DATA_ADDR(ht, new_data);	//将哈希表arData指向新内存区头部
			memcpy(ht->arData, old_buckets, sizeof(Bucket) * ht->nNumUsed);	//复制旧数据到新数据的内存区
			pefree(old_data, ht->u.flags & HASH_FLAG_PERSISTENT & HASH_FLAG_PERSISTENT);	//释放旧数据占用内存
			HT_HASH_RESET_PACKED(ht);	//重置哈希表的索引区
		} else {
			zend_hash_rehash(ht);//renumber为空，则重新计算hash
		}
	}

	return SUCCESS;
}

/**
 * @description: 对比两个哈希表
 * @param HashTable* ht1 第一个哈希表
 * @param HashTable* ht2 第二个哈希表
 * @param compare_func_t compar 对比函数
 * @return: int ht1大于h52返回1，否则返回-1
 */
static zend_always_inline int zend_hash_compare_impl(HashTable *ht1, HashTable *ht2, compare_func_t compar, zend_bool ordered) {
	uint32_t idx1, idx2;

	//先对比有效元素数
	if (ht1->nNumOfElements != ht2->nNumOfElements) {
		return ht1->nNumOfElements > ht2->nNumOfElements ? 1 : -1;
	}

	for (idx1 = 0, idx2 = 0; idx1 < ht1->nNumUsed; idx1++) {
		Bucket *p1 = ht1->arData + idx1, *p2;
		zval *pData1, *pData2;
		int result;

		if (Z_TYPE(p1->val) == IS_UNDEF) continue;	//跳过无效元素
		if (ordered) {
			//从ht2总找到第一个有效元素
			while (1) {
				ZEND_ASSERT(idx2 != ht2->nNumUsed);
				p2 = ht2->arData + idx2;
				if (Z_TYPE(p2->val) != IS_UNDEF) break;
				idx2++;
			}

			//如果两个元素索引都是数值，则对比索引
			if (p1->key == NULL && p2->key == NULL) { /* numeric indices */
				if (p1->h != p2->h) {
					return p1->h > p2->h ? 1 : -1;
				}
			} else if (p1->key != NULL && p2->key != NULL) { /* string indices */
				//如果两个索引的类型是字符串索引，则先对比字符串长度
				if (ZSTR_LEN(p1->key) != ZSTR_LEN(p2->key)) {
					return ZSTR_LEN(p1->key) > ZSTR_LEN(p2->key) ? 1 : -1;
				}
				//然后再对比内存
				result = memcmp(ZSTR_VAL(p1->key), ZSTR_VAL(p2->key), ZSTR_LEN(p1->key));
				if (result != 0) {
					return result;
				}
			} else {
				/* Mixed key types: A string key is considered as larger */
				return p1->key != NULL ? 1 : -1;
			}
			pData2 = &p2->val;
			idx2++;
		} else {
			if (p1->key == NULL) { /* numeric index */
				pData2 = zend_hash_index_find(ht2, p1->h); //如果数值索引在ht2中不存在，返回1
				if (pData2 == NULL) {
					return 1;
				}
			} else { /* string index */
				pData2 = zend_hash_find(ht2, p1->key);	//如果字符索引在ht2中不存在，返回1
				if (pData2 == NULL) {
					return 1;
				}
			}
		}

		pData1 = &p1->val;
		//如果两个哈希表的元素是间接类型，则用实际值进行对比
		if (Z_TYPE_P(pData1) == IS_INDIRECT) {
			pData1 = Z_INDIRECT_P(pData1);
		}
		if (Z_TYPE_P(pData2) == IS_INDIRECT) {
			pData2 = Z_INDIRECT_P(pData2);
		}

		if (Z_TYPE_P(pData1) == IS_UNDEF) {
			if (Z_TYPE_P(pData2) != IS_UNDEF) {
				return -1;	//如果ht1元素是无效，而ht2元素是有效，则返回-1
			}
		} else if (Z_TYPE_P(pData2) == IS_UNDEF) {
			return 1; //如果ht1元素有效，而ht2元素无效，则返回1
		} else {
			result = compar(pData1, pData2);	//使用对比函数进行对比两个元素
			if (result != 0) {
				return result;
			}
		}
	}

	return 0;
}

/**
 * @description: 对比两个哈希表
 * @param HashTable* ht1 第一个哈希表
 * @param HashTable* ht2 第二个哈希表
 * @param compare_func_t compar 对比函数
 * @return: int ht1大于h52返回1，否则返回-1
 */
ZEND_API int zend_hash_compare(HashTable *ht1, HashTable *ht2, compare_func_t compar, zend_bool ordered)
{
	int result;
	IS_CONSISTENT(ht1);
	IS_CONSISTENT(ht2);

	HASH_PROTECT_RECURSION(ht1);
	HASH_PROTECT_RECURSION(ht2);
	result = zend_hash_compare_impl(ht1, ht2, compar, ordered);
	HASH_UNPROTECT_RECURSION(ht1);
	HASH_UNPROTECT_RECURSION(ht2);

	return result;
}

/**
 * @description: 获取哈希表最大或者最小的元素
 * @param HashTable* ht 待操作哈希表
 * @param compare_func_t compar 对比函数
 * @param uint32_t flag 大于0取最大，否则取最小
 * @return: 
 */
ZEND_API zval* ZEND_FASTCALL zend_hash_minmax(const HashTable *ht, compare_func_t compar, uint32_t flag)
{
	uint32_t idx;
	Bucket *p, *res;

	IS_CONSISTENT(ht);

	if (ht->nNumOfElements == 0 ) {
		return NULL;
	}

	idx = 0;
	//取到第一个有效元素的索引
	while (1) {
		if (idx == ht->nNumUsed) {
			return NULL;
		}
		if (Z_TYPE(ht->arData[idx].val) != IS_UNDEF) break;
		idx++;
	}
	res = ht->arData + idx; //res为比较基准
	for (; idx < ht->nNumUsed; idx++) {
		p = ht->arData + idx;
		if (UNEXPECTED(Z_TYPE(p->val) == IS_UNDEF)) continue;	//跳过无效元素

		if (flag) {
			if (compar(res, p) < 0) { /* max */
				res = p;
			}
		} else {
			if (compar(res, p) > 0) { /* min */
				res = p;
			}
		}
	}
	return &res->val;
}

/**
 * @description: 将字符串转换成数值并赋值给idx
 * @param char* key 待操作字符串
 * @param size_t length 字符串长度
 * @param zend_ulong* idx索引
 * @return: int
 */
ZEND_API int ZEND_FASTCALL _zend_handle_numeric_str_ex(const char *key, size_t length, zend_ulong *idx)
{
	register const char *tmp = key;

	const char *end = key + length;

	if (*tmp == '-') {
		tmp++;
	}

	if ((*tmp == '0' && length > 1) /* numbers with leading zeros */
	 || (end - tmp > MAX_LENGTH_OF_LONG - 1) /* number too long */
	 || (SIZEOF_ZEND_LONG == 4 &&
	     end - tmp == MAX_LENGTH_OF_LONG - 1 &&
	     *tmp > '2')) { /* overflow */
		return 0;	//字符串长度超过系统架构支持最大的长整型数字个，或者值范围超过长整型范围，则返回0
	}
	*idx = (*tmp - '0');
	while (1) {
		++tmp;
		if (tmp == end) {
			if (*key == '-') {
				if (*idx-1 > ZEND_LONG_MAX) { /* overflow */
					return 0;	//如果idx的值-1溢出了，则返回0
				}
				*idx = 0 - *idx;	//否则返回0-idx
			} else if (*idx > ZEND_LONG_MAX) { /* overflow */
				return 0;	//如果idx溢出，则返回0
			}
			return 1;	//否则返回1
		}
		if (*tmp <= '9' && *tmp >= '0') {
			*idx = (*idx * 10) + (*tmp - '0');	//转换为整形
		} else {
			return 0;
		}
	}
}

/* Takes a "symtable" hashtable (contains integer and non-numeric string keys)
 * and converts it to a "proptable" (contains only string keys).
 * If the symtable didn't need duplicating, its refcount is incremented.
 */
ZEND_API HashTable* ZEND_FASTCALL zend_symtable_to_proptable(HashTable *ht)
{
	zend_ulong num_key;
	zend_string *str_key;
	zval *zv;

	if (UNEXPECTED(HT_IS_PACKED(ht))) { //数值索引哈希表，去转换成字符串索引哈希表
		goto convert;
	}

	ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, str_key, zv) {
		if (!str_key) {
			goto convert;	//如果字符串key不为空
		}
	} ZEND_HASH_FOREACH_END();

	if (!(GC_FLAGS(ht) & IS_ARRAY_IMMUTABLE)) {	//非固定数组哈希表，则gc.refcount增加
		GC_REFCOUNT(ht)++;
	}

	return ht;

convert:
	{
		HashTable *new_ht = emalloc(sizeof(HashTable));	//初始化新哈希表

		zend_hash_init(new_ht, zend_hash_num_elements(ht), NULL, ZVAL_PTR_DTOR, 0);	//初始化新哈希表元素，数量为旧哈希表有效元素个数

		//遍历哈希表
		ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, str_key, zv) {
			if (!str_key) {
				str_key = zend_long_to_str(num_key);	//将数字索引转换为字符串索引
				zend_string_delref(str_key);	//字符串gc计数减少
			}
			do {
				if (Z_OPT_REFCOUNTED_P(zv)) {
					//如果是应用类型，并且计数为1
					if (Z_ISREF_P(zv) && Z_REFCOUNT_P(zv) == 1) {
						zv = Z_REFVAL_P(zv);	//实际值不是引用类型，则跳过
						if (!Z_OPT_REFCOUNTED_P(zv)) {
							break;
						}
					}
					Z_ADDREF_P(zv);	//增加引用的引用计数
				}
			} while (0);
			zend_hash_update(new_ht, str_key, zv);	//更新新哈希表的字符串key为zv
		} ZEND_HASH_FOREACH_END();

		return new_ht;
	}
}

/* Takes a "proptable" hashtable (contains only string keys) and converts it to
 * a "symtable" (contains integer and non-numeric string keys).
 * If the proptable didn't need duplicating, its refcount is incremented.
 */
ZEND_API HashTable* ZEND_FASTCALL zend_proptable_to_symtable(HashTable *ht, zend_bool always_duplicate)
{
	zend_ulong num_key;
	zend_string *str_key;
	zval *zv;

	ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, str_key, zv) {
		/* The `str_key &&` here might seem redundant: property tables should
		 * only have string keys. Unfortunately, this isn't true, at the very
		 * least because of ArrayObject, which stores a symtable where the
		 * property table should be.
		 */
		//此处实际调用_zend_handle_numeric_str_ex这个函数将字符串转换成数值并赋值给num_key
		if (str_key && ZEND_HANDLE_NUMERIC(str_key, num_key)) {	//如果字符串key存在，并且转换整数值索引成功，则goto到转换哈希表操作
			goto convert;
		}
	} ZEND_HASH_FOREACH_END();

	if (always_duplicate) {
		return zend_array_dup(ht);	//如果always_duplicate参数为真，则新复制一个新的哈希表并返回
	}

	if (EXPECTED(!(GC_FLAGS(ht) & IS_ARRAY_IMMUTABLE))) {	//如果哈希表是固定数组，则直接gc计数加1并返回
		GC_REFCOUNT(ht)++;
	}

	return ht;

convert:
	{
		HashTable *new_ht = emalloc(sizeof(HashTable));	//新申请一个哈希表

		zend_hash_init(new_ht, zend_hash_num_elements(ht), NULL, ZVAL_PTR_DTOR, 0);	//初始化新哈希表

		ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, str_key, zv) {
			do {
				if (Z_OPT_REFCOUNTED_P(zv)) {
					//如果是应用类型，并且计数为1
					if (Z_ISREF_P(zv) && Z_REFCOUNT_P(zv) == 1) {
						zv = Z_REFVAL_P(zv);
						if (!Z_OPT_REFCOUNTED_P(zv)) {//实际值不是引用类型，则跳过
							break;
						}
					}
					Z_ADDREF_P(zv);//增加引用的引用计数
				}
			} while (0);
			/* Again, thank ArrayObject for `!str_key ||`. */
			if (!str_key || ZEND_HANDLE_NUMERIC(str_key, num_key)) {
				zend_hash_index_update(new_ht, num_key, zv);
			} else {
				zend_hash_update(new_ht, str_key, zv);
			}
		} ZEND_HASH_FOREACH_END();

		return new_ht;
	}
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
