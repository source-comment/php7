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
   |          Xinchen Hui <xinchen.h@zend.com>                            |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_TYPES_H
#define ZEND_TYPES_H

#include "zend_portability.h"
#include "zend_long.h"

#ifdef WORDS_BIGENDIAN
# define ZEND_ENDIAN_LOHI(lo, hi)          hi; lo;
# define ZEND_ENDIAN_LOHI_3(lo, mi, hi)    hi; mi; lo;
# define ZEND_ENDIAN_LOHI_4(a, b, c, d)    d; c; b; a;
# define ZEND_ENDIAN_LOHI_C(lo, hi)        hi, lo
# define ZEND_ENDIAN_LOHI_C_3(lo, mi, hi)  hi, mi, lo,
# define ZEND_ENDIAN_LOHI_C_4(a, b, c, d)  d, c, b, a
#else
# define ZEND_ENDIAN_LOHI(lo, hi)          lo; hi;
# define ZEND_ENDIAN_LOHI_3(lo, mi, hi)    lo; mi; hi;
# define ZEND_ENDIAN_LOHI_4(a, b, c, d)    a; b; c; d;
# define ZEND_ENDIAN_LOHI_C(lo, hi)        lo, hi
# define ZEND_ENDIAN_LOHI_C_3(lo, mi, hi)  lo, mi, hi,
# define ZEND_ENDIAN_LOHI_C_4(a, b, c, d)  a, b, c, d
#endif

typedef unsigned char zend_bool;
typedef unsigned char zend_uchar;

typedef enum {
  SUCCESS =  0,
  FAILURE = -1,		/* this MUST stay a negative number, or it may affect functions! */
} ZEND_RESULT_CODE;

#ifdef ZEND_ENABLE_ZVAL_LONG64
# ifdef ZEND_WIN32
#  define ZEND_SIZE_MAX  _UI64_MAX
# else
#  define ZEND_SIZE_MAX  SIZE_MAX
# endif
#else
# if defined(ZEND_WIN32)
#  define ZEND_SIZE_MAX  _UI32_MAX
# else
#  define ZEND_SIZE_MAX SIZE_MAX
# endif
#endif

typedef intptr_t zend_intptr_t;
typedef uintptr_t zend_uintptr_t;

#ifdef ZTS
#define ZEND_TLS static TSRM_TLS
#define ZEND_EXT_TLS TSRM_TLS
#else
#define ZEND_TLS static
#define ZEND_EXT_TLS
#endif

typedef struct _zend_object_handlers zend_object_handlers;
typedef struct _zend_class_entry     zend_class_entry;
typedef union  _zend_function        zend_function;
typedef struct _zend_execute_data    zend_execute_data;

typedef struct _zval_struct     zval;

typedef struct _zend_refcounted zend_refcounted;
typedef struct _zend_string     zend_string;
typedef struct _zend_array      zend_array;
typedef struct _zend_object     zend_object;
typedef struct _zend_resource   zend_resource;
typedef struct _zend_reference  zend_reference;
typedef struct _zend_ast_ref    zend_ast_ref;
typedef struct _zend_ast        zend_ast;

typedef int  (*compare_func_t)(const void *, const void *);
typedef void (*swap_func_t)(void *, void *);
typedef void (*sort_func_t)(void *, size_t, size_t, compare_func_t, swap_func_t);
typedef void (*dtor_func_t)(zval *pDest);
typedef void (*copy_ctor_func_t)(zval *pElement);

/*
 * zend_type - is an abstraction layer to represent information about type hint.
 * It shouldn't be used directly. Only through ZEND_TYPE_* macros.
 *
 * ZEND_TYPE_IS_SET()     - checks if type-hint exists
 * ZEND_TYPE_IS_CODE()    - checks if type-hint refer to standard type
 * ZEND_TYPE_IS_CLASS()   - checks if type-hint refer to some class
 *
 * ZEND_TYPE_NAME()       - returns referenced class name
 * ZEND_TYPE_CE()         - returns referenced class entry
 * ZEND_TYPE_CODE()       - returns standard type code (e.g. IS_LONG, _IS_BOOL)
 *
 * ZEND_TYPE_ALLOW_NULL() - checks if NULL is allowed
 *
 * ZEND_TYPE_ENCODE() and ZEND_TYPE_ENCODE_CLASS() should be used for
 * construction.
 */

typedef uintptr_t zend_type;

#define ZEND_TYPE_IS_SET(t) \
	((t) > Z_L(1))

#define ZEND_TYPE_IS_CODE(t) \
	(((t) > Z_L(1)) && ((t) <= Z_L(0x1ff)))

#define ZEND_TYPE_IS_CLASS(t) \
	((t) > Z_L(0x1ff))

#define ZEND_TYPE_NAME(t) \
	((zend_string*)((t) & ~Z_L(0x3)))

#define ZEND_TYPE_CE(t) \
	((zend_class_entry*)((t) & ~Z_L(0x3)))

#define ZEND_TYPE_CODE(t) \
	((t) >> Z_L(1))

#define ZEND_TYPE_ALLOW_NULL(t) \
	(((t) & Z_L(0x1)) != 0)

#define ZEND_TYPE_ENCODE(code, allow_null) \
	(((code) << Z_L(1)) | ((allow_null) ? Z_L(0x1) : Z_L(0x0)))

#define ZEND_TYPE_ENCODE_CLASS(class_name, allow_null) \
	(((uintptr_t)(class_name)) | ((allow_null) ? Z_L(0x1) : Z_L(0)))

#define ZEND_TYPE_ENCODE_CLASS_CONST_0(class_name) \
	((zend_type) class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST_1(class_name) \
	((zend_type) "?" class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST_Q2(macro, class_name) \
	macro(class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST_Q1(allow_null, class_name) \
	ZEND_TYPE_ENCODE_CLASS_CONST_Q2(ZEND_TYPE_ENCODE_CLASS_CONST_ ##allow_null, class_name)
#define ZEND_TYPE_ENCODE_CLASS_CONST(class_name, allow_null) \
	ZEND_TYPE_ENCODE_CLASS_CONST_Q1(allow_null, class_name)

/*zval.value结构*/
typedef union _zend_value {
	zend_long         lval;				/* long value */
	double            dval;				/* double value */
	zend_refcounted  *counted;
	zend_string      *str;	//字符串指针
	zend_array       *arr;	//数组指针
	zend_object      *obj;	//对象指针
	zend_resource    *res;	//资源指针
	zend_reference   *ref;	//引用指针
	zend_ast_ref     *ast;	//语法树指针
	zval             *zv;	//zval指针
	void             *ptr;	//其他类型指针
	zend_class_entry *ce;	//类指针
	zend_function    *func;	//函数指针
	struct {
		uint32_t w1;
		uint32_t w2;
	} ww;
} zend_value;

//基础变量结构体
struct _zval_struct {
	zend_value        value;			/* value 变量值的结构体*/	
	union {
		struct {
			ZEND_ENDIAN_LOHI_4(
				zend_uchar    type,			/* active type */
				zend_uchar    type_flags,
				zend_uchar    const_flags,
				zend_uchar    reserved)	    /* call info for EX(This) */
		} v;
		uint32_t type_info;	//类型
	} u1;
	union {
		uint32_t     next;                 /* hash collision chain */
		uint32_t     cache_slot;           /* literal cache slot */
		uint32_t     lineno;               /* line number (for ast nodes) */
		uint32_t     num_args;             /* arguments number for EX(This) */
		uint32_t     fe_pos;               /* foreach position */
		uint32_t     fe_iter_idx;          /* foreach iterator index */
		uint32_t     access_flags;         /* class constant access flags */
		uint32_t     property_guard;       /* single property guard */
		uint32_t     extra;                /* not further specified */
	} u2;
};

/**GC引用结构体*/
typedef struct _zend_refcounted_h {
	uint32_t         refcount;			/* reference counter 32-bit */
	union {
		struct {
			ZEND_ENDIAN_LOHI_3(
				zend_uchar    type,
				zend_uchar    flags,    /* used for strings & objects */
				uint16_t      gc_info)  /* keeps GC root number (or 0) and color */
		} v;
		uint32_t type_info;
	} u;
} zend_refcounted_h;

//引用计数
struct _zend_refcounted {
	zend_refcounted_h gc;
};

/**字符串结构*/
struct _zend_string {
	zend_refcounted_h gc;	//引用计数信息
	zend_ulong        h;                /* hash value */
	size_t            len;	//字符串长度
	char              val[1];	//字符串内容，柔性数组
};

/**哈希表元素结构*/
typedef struct _Bucket {
	zval              val;	//元素值
	zend_ulong        h;                /* hash value (or numeric index)   */
	zend_string      *key;              /* string key or NULL for numerics */
} Bucket;

//哈希表类型
typedef struct _zend_array HashTable;

struct _zend_array {
	zend_refcounted_h gc;	//GC计数信息
	union {
		struct {
			ZEND_ENDIAN_LOHI_4(
				zend_uchar    flags,	//哈希表类型
				zend_uchar    nApplyCount,	//递归深度
				zend_uchar    nIteratorsCount,	//迭代位置
				zend_uchar    consistency)
		} v;
		uint32_t flags;
	} u;
	uint32_t          nTableMask;	//哈希表掩码，数值哈希表-2，普通哈希表-nTableSize
	Bucket           *arData;	//哈希表第一个元素指针
	uint32_t          nNumUsed;	//已用元素数量
	uint32_t          nNumOfElements;	//有效元素数量
	uint32_t          nTableSize;	//哈希表大小
	uint32_t          nInternalPointer;	//内部指针
	zend_long         nNextFreeElement;	//下一个插入元素的位置
	dtor_func_t       pDestructor;	//析构函数
};

/*
 * HashTable Data Layout
 * =====================
 *
 *                 +=============================+
 *                 | HT_HASH(ht, ht->nTableMask) |
 *                 | ...                         |
 *                 | HT_HASH(ht, -1)             |
 *                 +-----------------------------+
 * ht->arData ---> | Bucket[0]                   |
 *                 | ...                         |
 *                 | Bucket[ht->nTableSize-1]    |
 *                 +=============================+
 */

#define HT_INVALID_IDX ((uint32_t) -1)	//无效索引

#define HT_MIN_MASK ((uint32_t) -2)	//哈希表最小掩码，用于数值索引
#define HT_MIN_SIZE 8	//普通索引哈希表最小元素数

#if SIZEOF_SIZE_T == 4
# define HT_MAX_SIZE 0x04000000 /* small enough to avoid overflow checks */
# define HT_HASH_TO_BUCKET_EX(data, idx) \
	((Bucket*)((char*)(data) + (idx)))
# define HT_IDX_TO_HASH(idx) \
	((idx) * sizeof(Bucket))
# define HT_HASH_TO_IDX(idx) \
	((idx) / sizeof(Bucket))
#elif SIZEOF_SIZE_T == 8
# define HT_MAX_SIZE 0x80000000
# define HT_HASH_TO_BUCKET_EX(data, idx) \
	((data) + (idx))
# define HT_IDX_TO_HASH(idx) \
	(idx)
# define HT_HASH_TO_IDX(idx) \
	(idx)
#else
# error "Unknown SIZEOF_SIZE_T"
#endif

//获取idx位置的元素
#define HT_HASH_EX(data, idx) \
	((uint32_t*)(data))[(int32_t)(idx)]

//获取哈希表idx位置的元素
#define HT_HASH(ht, idx) \
	HT_HASH_EX((ht)->arData, idx)

//索引区大小
#define HT_HASH_SIZE(nTableMask) \
	(((size_t)(uint32_t)-(int32_t)(nTableMask)) * sizeof(uint32_t))

//数据区大小
#define HT_DATA_SIZE(nTableSize) \
	((size_t)(nTableSize) * sizeof(Bucket))

//arData总大小
#define HT_SIZE_EX(nTableSize, nTableMask) \
	(HT_DATA_SIZE((nTableSize)) + HT_HASH_SIZE((nTableMask)))

//哈希表总大小，索引区+数据区
#define HT_SIZE(ht) \
	HT_SIZE_EX((ht)->nTableSize, (ht)->nTableMask)

//已用索引区+数据区大小
#define HT_USED_SIZE(ht) \
	(HT_HASH_SIZE((ht)->nTableMask) + ((size_t)(ht)->nNumUsed * sizeof(Bucket)))
	
//重置索引区为无效值
#define HT_HASH_RESET(ht) \
	memset(&HT_HASH(ht, (ht)->nTableMask), HT_INVALID_IDX, HT_HASH_SIZE((ht)->nTableMask))

//重置数值索引哈希表索引区
#define HT_HASH_RESET_PACKED(ht) do { \
		HT_HASH(ht, -2) = HT_INVALID_IDX; \
		HT_HASH(ht, -1) = HT_INVALID_IDX; \
	} while (0)

//索引位置的数据
#define HT_HASH_TO_BUCKET(ht, idx) \
	HT_HASH_TO_BUCKET_EX((ht)->arData, idx)

//哈希表数据指针指向数据区第一个位置
#define HT_SET_DATA_ADDR(ht, ptr) do { \
		(ht)->arData = (Bucket*)(((char*)(ptr)) + HT_HASH_SIZE((ht)->nTableMask)); \
	} while (0)

//哈希表指针移动到内存首地址
#define HT_GET_DATA_ADDR(ht) \
	((char*)((ht)->arData) - HT_HASH_SIZE((ht)->nTableMask))

typedef uint32_t HashPosition;

//迭代器结构
typedef struct _HashTableIterator {
	HashTable    *ht;	//哈希表
	HashPosition  pos;	//哈希表位置
} HashTableIterator;

//对象结构体
struct _zend_object {
	zend_refcounted_h gc;	//GC计数信息
	uint32_t          handle; // TODO: may be removed ???
	zend_class_entry *ce;	//所属类
	const zend_object_handlers *handlers;	//对象操作函数表
	HashTable        *properties;	//属性名表
	zval              properties_table[1];	//属性数据表
};

//资源结构体
struct _zend_resource {
	zend_refcounted_h gc;	//GC计数信息
	int               handle; // TODO: may be removed ???
	int               type;	//类型
	void             *ptr;	//资源指针
};

//引用结构体
struct _zend_reference {
	zend_refcounted_h gc;	//GC计数信息
	zval              val;	//数据
};

//语法树引用结构体
struct _zend_ast_ref {
	zend_refcounted_h gc;	//GC计数信息
	zend_ast         *ast;	//树节点
};

/* regular data types */
#define IS_UNDEF					0
#define IS_NULL						1
#define IS_FALSE					2
#define IS_TRUE						3
#define IS_LONG						4
#define IS_DOUBLE					5
#define IS_STRING					6
#define IS_ARRAY					7
#define IS_OBJECT					8
#define IS_RESOURCE					9
#define IS_REFERENCE				10

/* constant expressions */
#define IS_CONSTANT					11
#define IS_CONSTANT_AST				12

/* fake types */
#define _IS_BOOL					13
#define IS_CALLABLE					14
#define IS_ITERABLE					19
#define IS_VOID						18

/* internal types */
#define IS_INDIRECT             	15
#define IS_PTR						17
#define _IS_ERROR					20

/**
 * @description: 返回变量类型
 * @param zval* pz 变量指针
 * @return: zend_uchar 类型
 */
static zend_always_inline zend_uchar zval_get_type(const zval* pz) {
	return pz->u1.v.type;
}

#define ZEND_SAME_FAKE_TYPE(faketype, realtype) ( \
	(faketype) == (realtype) \
	|| ((faketype) == _IS_BOOL && ((realtype) == IS_TRUE || (realtype) == IS_FALSE)) \
)

/* we should never set just Z_TYPE, we should set Z_TYPE_INFO */
#define Z_TYPE(zval)				zval_get_type(&(zval))	//zval类型
#define Z_TYPE_P(zval_p)			Z_TYPE(*(zval_p))	//zval指针的类型

#define Z_TYPE_FLAGS(zval)			(zval).u1.v.type_flags	//zval类型掩码
#define Z_TYPE_FLAGS_P(zval_p)		Z_TYPE_FLAGS(*(zval_p))	//zval指针的类型掩码

#define Z_CONST_FLAGS(zval)			(zval).u1.v.const_flags	//
#define Z_CONST_FLAGS_P(zval_p)		Z_CONST_FLAGS(*(zval_p))

#define Z_TYPE_INFO(zval)			(zval).u1.type_info
#define Z_TYPE_INFO_P(zval_p)		Z_TYPE_INFO(*(zval_p))

#define Z_NEXT(zval)				(zval).u2.next	//zval下一个索引位
#define Z_NEXT_P(zval_p)			Z_NEXT(*(zval_p))	//zval指针的下一个索引位

#define Z_CACHE_SLOT(zval)			(zval).u2.cache_slot	//zval的缓存槽
#define Z_CACHE_SLOT_P(zval_p)		Z_CACHE_SLOT(*(zval_p)) //zval指针的缓存槽

#define Z_FE_POS(zval)				(zval).u2.fe_pos	//zval的遍历位置
#define Z_FE_POS_P(zval_p)			Z_FE_POS(*(zval_p))	//zval指针的遍历位置

#define Z_FE_ITER(zval)				(zval).u2.fe_iter_idx	//zval的迭代器位置
#define Z_FE_ITER_P(zval_p)			Z_FE_ITER(*(zval_p))	//zval指针的迭代器位置

#define Z_ACCESS_FLAGS(zval)		(zval).u2.access_flags	//
#define Z_ACCESS_FLAGS_P(zval_p)	Z_ACCESS_FLAGS(*(zval_p))

#define Z_EXTRA(zval)				(zval).u2.extra
#define Z_EXTRA_P(zval_p)			Z_EXTRA(*(zval_p))

#define Z_COUNTED(zval)				(zval).value.counted	//zval的计数信息
#define Z_COUNTED_P(zval_p)			Z_COUNTED(*(zval_p))	//zval指针的计数信息

#define Z_TYPE_MASK					0xff

#define Z_TYPE_FLAGS_SHIFT			8
#define Z_CONST_FLAGS_SHIFT			16

//GC属性操作，读或写
#define GC_REFCOUNT(p)				(p)->gc.refcount
#define GC_TYPE(p)					(p)->gc.u.v.type
#define GC_FLAGS(p)					(p)->gc.u.v.flags
#define GC_INFO(p)					(p)->gc.u.v.gc_info
#define GC_TYPE_INFO(p)				(p)->gc.u.type_info

//变量的gc属性操作
#define Z_GC_TYPE(zval)				GC_TYPE(Z_COUNTED(zval))
#define Z_GC_TYPE_P(zval_p)			Z_GC_TYPE(*(zval_p))

#define Z_GC_FLAGS(zval)			GC_FLAGS(Z_COUNTED(zval))
#define Z_GC_FLAGS_P(zval_p)		Z_GC_FLAGS(*(zval_p))

#define Z_GC_INFO(zval)				GC_INFO(Z_COUNTED(zval))
#define Z_GC_INFO_P(zval_p)			Z_GC_INFO(*(zval_p))
#define Z_GC_TYPE_INFO(zval)		GC_TYPE_INFO(Z_COUNTED(zval))
#define Z_GC_TYPE_INFO_P(zval_p)	Z_GC_TYPE_INFO(*(zval_p))

#define GC_FLAGS_SHIFT				8
#define GC_INFO_SHIFT				16
#define GC_INFO_MASK				0xffff0000

/* zval.value->gc.u.v.flags */
#define GC_COLLECTABLE				(1<<7)

#define GC_ARRAY					(IS_ARRAY          | (GC_COLLECTABLE << GC_FLAGS_SHIFT))
#define GC_OBJECT					(IS_OBJECT         | (GC_COLLECTABLE << GC_FLAGS_SHIFT))

/* zval.u1.v.type_flags */
#define IS_TYPE_CONSTANT			(1<<0)	//常量
#define IS_TYPE_REFCOUNTED			(1<<2)	//引用
#define IS_TYPE_COPYABLE			(1<<4)	//写时复制

/* extended types */
#define IS_INTERNED_STRING_EX		IS_STRING

#define IS_STRING_EX				(IS_STRING         | ((                   IS_TYPE_REFCOUNTED | IS_TYPE_COPYABLE) << Z_TYPE_FLAGS_SHIFT))
#define IS_ARRAY_EX					(IS_ARRAY          | ((                   IS_TYPE_REFCOUNTED | IS_TYPE_COPYABLE) << Z_TYPE_FLAGS_SHIFT))
#define IS_OBJECT_EX				(IS_OBJECT         | ((                   IS_TYPE_REFCOUNTED                   ) << Z_TYPE_FLAGS_SHIFT))
#define IS_RESOURCE_EX				(IS_RESOURCE       | ((                   IS_TYPE_REFCOUNTED                   ) << Z_TYPE_FLAGS_SHIFT))
#define IS_REFERENCE_EX				(IS_REFERENCE      | ((                   IS_TYPE_REFCOUNTED                   ) << Z_TYPE_FLAGS_SHIFT))

#define IS_CONSTANT_EX				(IS_CONSTANT       | ((IS_TYPE_CONSTANT | IS_TYPE_REFCOUNTED | IS_TYPE_COPYABLE) << Z_TYPE_FLAGS_SHIFT))
#define IS_CONSTANT_AST_EX			(IS_CONSTANT_AST   | ((IS_TYPE_CONSTANT | IS_TYPE_REFCOUNTED | IS_TYPE_COPYABLE) << Z_TYPE_FLAGS_SHIFT))

/* zval.u1.v.const_flags */
#define IS_CONSTANT_UNQUALIFIED		0x010
#define IS_CONSTANT_VISITED_MARK	0x020
#define IS_CONSTANT_CLASS           0x080  /* __CLASS__ in trait */
#define IS_CONSTANT_IN_NAMESPACE	0x100  /* used only in opline->extended_value */

#define IS_CONSTANT_VISITED(p)		(Z_CONST_FLAGS_P(p) & IS_CONSTANT_VISITED_MARK)
#define MARK_CONSTANT_VISITED(p)	Z_CONST_FLAGS_P(p) |= IS_CONSTANT_VISITED_MARK
#define RESET_CONSTANT_VISITED(p)	Z_CONST_FLAGS_P(p) &= ~IS_CONSTANT_VISITED_MARK

/* string flags (zval.value->gc.u.flags) */
#define IS_STR_PERSISTENT			(1<<0) /* allocated using malloc   永久字符串，直接向系统申请开辟内存*/
#define IS_STR_INTERNED				(1<<1) /* interned string        内部字符串  */
#define IS_STR_PERMANENT        	(1<<2) /* relives request boundary */

#define IS_STR_CONSTANT             (1<<3) /* constant index 常量字符串*/
#define IS_STR_CONSTANT_UNQUALIFIED (1<<4) /* the same as IS_CONSTANT_UNQUALIFIED 常量字符串，可能带有命名空间*/

/* array flags */
#define IS_ARRAY_IMMUTABLE			(1<<1)	//固定数组

/* object flags (zval.value->gc.u.flags) */
#define IS_OBJ_APPLY_COUNT			0x07	//递归保护
#define IS_OBJ_DESTRUCTOR_CALLED	(1<<3)	//析构函数已调用
#define IS_OBJ_FREE_CALLED			(1<<4)	//清理函数已调用
#define IS_OBJ_USE_GUARDS           (1<<5)	//魔术方法递归保护
#define IS_OBJ_HAS_GUARDS           (1<<6)	//是否有魔术方法递归保护

//变量是否递归保护
#define Z_OBJ_APPLY_COUNT(zval) \
	(Z_GC_FLAGS(zval) & IS_OBJ_APPLY_COUNT)

//递归加
#define Z_OBJ_INC_APPLY_COUNT(zval) do { \
		Z_GC_FLAGS(zval) = \
			(Z_GC_FLAGS(zval) & ~IS_OBJ_APPLY_COUNT) | \
			((Z_GC_FLAGS(zval) & IS_OBJ_APPLY_COUNT) + 1); \
	} while (0)

//递归减
#define Z_OBJ_DEC_APPLY_COUNT(zval) do { \
		Z_GC_FLAGS(zval) = \
			(Z_GC_FLAGS(zval) & ~IS_OBJ_APPLY_COUNT) | \
			((Z_GC_FLAGS(zval) & IS_OBJ_APPLY_COUNT) - 1); \
	} while (0)

//指针变量的递归
#define Z_OBJ_APPLY_COUNT_P(zv)     Z_OBJ_APPLY_COUNT(*(zv))
//指针变量递归加
#define Z_OBJ_INC_APPLY_COUNT_P(zv) Z_OBJ_INC_APPLY_COUNT(*(zv))
//指针变量递归减
#define Z_OBJ_DEC_APPLY_COUNT_P(zv) Z_OBJ_DEC_APPLY_COUNT(*(zv))

/* All data types < IS_STRING have their constructor/destructors skipped */
//常量
#define Z_CONSTANT(zval)			((Z_TYPE_FLAGS(zval) & IS_TYPE_CONSTANT) != 0)
#define Z_CONSTANT_P(zval_p)		Z_CONSTANT(*(zval_p))
//引用
#define Z_REFCOUNTED(zval)			((Z_TYPE_FLAGS(zval) & IS_TYPE_REFCOUNTED) != 0)
#define Z_REFCOUNTED_P(zval_p)		Z_REFCOUNTED(*(zval_p))
//可复制
#define Z_COPYABLE(zval)			((Z_TYPE_FLAGS(zval) & IS_TYPE_COPYABLE) != 0)
#define Z_COPYABLE_P(zval_p)		Z_COPYABLE(*(zval_p))

/* deprecated: (IMMUTABLE is the same as COPYABLE && !REFCOUED) */
#define Z_IMMUTABLE(zval)			((Z_TYPE_FLAGS(zval) & (IS_TYPE_REFCOUNTED|IS_TYPE_COPYABLE)) == IS_TYPE_COPYABLE)
#define Z_IMMUTABLE_P(zval_p)		Z_IMMUTABLE(*(zval_p))
#define Z_OPT_IMMUTABLE(zval)		Z_IMMUTABLE(zval_p)
#define Z_OPT_IMMUTABLE_P(zval_p)	Z_IMMUTABLE(*(zval_p))

/* the following Z_OPT_* macros make better code when Z_TYPE_INFO accessed before */
#define Z_OPT_TYPE(zval)			(Z_TYPE_INFO(zval) & Z_TYPE_MASK)
#define Z_OPT_TYPE_P(zval_p)		Z_OPT_TYPE(*(zval_p))

#define Z_OPT_CONSTANT(zval)		((Z_TYPE_INFO(zval) & (IS_TYPE_CONSTANT << Z_TYPE_FLAGS_SHIFT)) != 0)
#define Z_OPT_CONSTANT_P(zval_p)	Z_OPT_CONSTANT(*(zval_p))

#define Z_OPT_REFCOUNTED(zval)		((Z_TYPE_INFO(zval) & (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) != 0)
#define Z_OPT_REFCOUNTED_P(zval_p)	Z_OPT_REFCOUNTED(*(zval_p))

#define Z_OPT_COPYABLE(zval)		((Z_TYPE_INFO(zval) & (IS_TYPE_COPYABLE << Z_TYPE_FLAGS_SHIFT)) != 0)
#define Z_OPT_COPYABLE_P(zval_p)	Z_OPT_COPYABLE(*(zval_p))

#define Z_OPT_ISREF(zval)			(Z_OPT_TYPE(zval) == IS_REFERENCE)
#define Z_OPT_ISREF_P(zval_p)		Z_OPT_ISREF(*(zval_p))

//变量类型是否引用
#define Z_ISREF(zval)				(Z_TYPE(zval) == IS_REFERENCE)
#define Z_ISREF_P(zval_p)			Z_ISREF(*(zval_p))

//变量类型是否IS_UNDEF
#define Z_ISUNDEF(zval)				(Z_TYPE(zval) == IS_UNDEF)
#define Z_ISUNDEF_P(zval_p)			Z_ISUNDEF(*(zval_p))

//变量类型是否IS_NULL
#define Z_ISNULL(zval)				(Z_TYPE(zval) == IS_NULL)
#define Z_ISNULL_P(zval_p)			Z_ISNULL(*(zval_p))

//变量类型是否IS_ERROR
#define Z_ISERROR(zval)				(Z_TYPE(zval) == _IS_ERROR)
#define Z_ISERROR_P(zval_p)			Z_ISERROR(*(zval_p))

//操作整形变量
#define Z_LVAL(zval)				(zval).value.lval
#define Z_LVAL_P(zval_p)			Z_LVAL(*(zval_p))

//操作浮点型变量
#define Z_DVAL(zval)				(zval).value.dval
#define Z_DVAL_P(zval_p)			Z_DVAL(*(zval_p))

//操作字符串变量
#define Z_STR(zval)					(zval).value.str
#define Z_STR_P(zval_p)				Z_STR(*(zval_p))

//操作字符串指针
#define Z_STRVAL(zval)				ZSTR_VAL(Z_STR(zval))
#define Z_STRVAL_P(zval_p)			Z_STRVAL(*(zval_p))

//操作字符串长度
#define Z_STRLEN(zval)				ZSTR_LEN(Z_STR(zval))
#define Z_STRLEN_P(zval_p)			Z_STRLEN(*(zval_p))

//操作字符串哈希值
#define Z_STRHASH(zval)				ZSTR_HASH(Z_STR(zval))
#define Z_STRHASH_P(zval_p)			Z_STRHASH(*(zval_p))

//操作数组
#define Z_ARR(zval)					(zval).value.arr
#define Z_ARR_P(zval_p)				Z_ARR(*(zval_p))

//操作数组
#define Z_ARRVAL(zval)				Z_ARR(zval)
#define Z_ARRVAL_P(zval_p)			Z_ARRVAL(*(zval_p))

//操作对象
#define Z_OBJ(zval)					(zval).value.obj
#define Z_OBJ_P(zval_p)				Z_OBJ(*(zval_p))

//操作对象函数表
#define Z_OBJ_HT(zval)				Z_OBJ(zval)->handlers
#define Z_OBJ_HT_P(zval_p)			Z_OBJ_HT(*(zval_p))

#define Z_OBJ_HANDLER(zval, hf)		Z_OBJ_HT((zval))->hf
#define Z_OBJ_HANDLER_P(zv_p, hf)	Z_OBJ_HANDLER(*(zv_p), hf)

#define Z_OBJ_HANDLE(zval)          (Z_OBJ((zval)))->handle
#define Z_OBJ_HANDLE_P(zval_p)      Z_OBJ_HANDLE(*(zval_p))

//操作对象类
#define Z_OBJCE(zval)				(Z_OBJ(zval)->ce)
#define Z_OBJCE_P(zval_p)			Z_OBJCE(*(zval_p))

//操作对象属性表
#define Z_OBJPROP(zval)				Z_OBJ_HT((zval))->get_properties(&(zval))
#define Z_OBJPROP_P(zval_p)			Z_OBJPROP(*(zval_p))

#define Z_OBJDEBUG(zval,tmp)		(Z_OBJ_HANDLER((zval),get_debug_info)?Z_OBJ_HANDLER((zval),get_debug_info)(&(zval),&tmp):(tmp=0,Z_OBJ_HANDLER((zval),get_properties)?Z_OBJPROP(zval):NULL))
#define Z_OBJDEBUG_P(zval_p,tmp)	Z_OBJDEBUG(*(zval_p), tmp)

//操作资源变量
#define Z_RES(zval)					(zval).value.res
#define Z_RES_P(zval_p)				Z_RES(*zval_p)

//操作资源句柄
#define Z_RES_HANDLE(zval)			Z_RES(zval)->handle
#define Z_RES_HANDLE_P(zval_p)		Z_RES_HANDLE(*zval_p)

//操作资源类型
#define Z_RES_TYPE(zval)			Z_RES(zval)->type
#define Z_RES_TYPE_P(zval_p)		Z_RES_TYPE(*zval_p)

//操作资源指针
#define Z_RES_VAL(zval)				Z_RES(zval)->ptr
#define Z_RES_VAL_P(zval_p)			Z_RES_VAL(*zval_p)

#define Z_REF(zval)					(zval).value.ref
#define Z_REF_P(zval_p)				Z_REF(*(zval_p))

//操作引用类型
#define Z_REFVAL(zval)				&Z_REF(zval)->val
#define Z_REFVAL_P(zval_p)			Z_REFVAL(*(zval_p))

//操作语法树
#define Z_AST(zval)					(zval).value.ast
#define Z_AST_P(zval_p)				Z_AST(*(zval_p))

//操作语法树
#define Z_ASTVAL(zval)				(zval).value.ast->ast
#define Z_ASTVAL_P(zval_p)			Z_ASTVAL(*(zval_p))

//操作间接变量
#define Z_INDIRECT(zval)			(zval).value.zv
#define Z_INDIRECT_P(zval_p)		Z_INDIRECT(*(zval_p))

//操作类变量
#define Z_CE(zval)					(zval).value.ce
#define Z_CE_P(zval_p)				Z_CE(*(zval_p))

//操作函数变量
#define Z_FUNC(zval)				(zval).value.func
#define Z_FUNC_P(zval_p)			Z_FUNC(*(zval_p))

//操作指针变量
#define Z_PTR(zval)					(zval).value.ptr
#define Z_PTR_P(zval_p)				Z_PTR(*(zval_p))

//设置变量为IS_UNDEF类型
#define ZVAL_UNDEF(z) do {				\
		Z_TYPE_INFO_P(z) = IS_UNDEF;	\
	} while (0)

//设置变量为IS_NULL类型
#define ZVAL_NULL(z) do {				\
		Z_TYPE_INFO_P(z) = IS_NULL;		\
	} while (0)

//设置变量为IS_FALSE
#define ZVAL_FALSE(z) do {				\
		Z_TYPE_INFO_P(z) = IS_FALSE;	\
	} while (0)

//设置变量为IS_TRUE
#define ZVAL_TRUE(z) do {				\
		Z_TYPE_INFO_P(z) = IS_TRUE;		\
	} while (0)

//设置变量的bool值
#define ZVAL_BOOL(z, b) do {			\
		Z_TYPE_INFO_P(z) =				\
			(b) ? IS_TRUE : IS_FALSE;	\
	} while (0)

//整形变量赋值
#define ZVAL_LONG(z, l) {				\
		zval *__z = (z);				\
		Z_LVAL_P(__z) = l;				\
		Z_TYPE_INFO_P(__z) = IS_LONG;	\
	}

//浮点型变量赋值
#define ZVAL_DOUBLE(z, d) {				\
		zval *__z = (z);				\
		Z_DVAL_P(__z) = d;				\
		Z_TYPE_INFO_P(__z) = IS_DOUBLE;	\
	}

//字符串变量赋值
#define ZVAL_STR(z, s) do {						\
		zval *__z = (z);						\
		zend_string *__s = (s);					\
		Z_STR_P(__z) = __s;						\
		/* interned strings support */			\
		Z_TYPE_INFO_P(__z) = ZSTR_IS_INTERNED(__s) ? \
			IS_INTERNED_STRING_EX : 			\
			IS_STRING_EX;						\
	} while (0)

//间接字符串赋值
#define ZVAL_INTERNED_STR(z, s) do {				\
		zval *__z = (z);							\
		zend_string *__s = (s);						\
		Z_STR_P(__z) = __s;							\
		Z_TYPE_INFO_P(__z) = IS_INTERNED_STRING_EX;	\
	} while (0)

//字符串变量新增
#define ZVAL_NEW_STR(z, s) do {					\
		zval *__z = (z);						\
		zend_string *__s = (s);					\
		Z_STR_P(__z) = __s;						\
		Z_TYPE_INFO_P(__z) = IS_STRING_EX;		\
	} while (0)

//字符串复制，支持内部字符串
#define ZVAL_STR_COPY(z, s) do {						\
		zval *__z = (z);								\
		zend_string *__s = (s);							\
		Z_STR_P(__z) = __s;								\
		/* interned strings support */					\
		if (ZSTR_IS_INTERNED(__s)) {							\
			Z_TYPE_INFO_P(__z) = IS_INTERNED_STRING_EX;	\
		} else {										\
			GC_REFCOUNT(__s)++;							\
			Z_TYPE_INFO_P(__z) = IS_STRING_EX;			\
		}												\
	} while (0)

//数组赋值
#define ZVAL_ARR(z, a) do {						\
		zval *__z = (z);						\
		Z_ARR_P(__z) = (a);						\
		Z_TYPE_INFO_P(__z) = IS_ARRAY_EX;		\
	} while (0)

//数组变量新增
#define ZVAL_NEW_ARR(z) do {									\
		zval *__z = (z);										\
		zend_array *_arr =										\
		(zend_array *) emalloc(sizeof(zend_array));				\
		Z_ARR_P(__z) = _arr;									\
		Z_TYPE_INFO_P(__z) = IS_ARRAY_EX;						\
	} while (0)

//新增持久化数组
#define ZVAL_NEW_PERSISTENT_ARR(z) do {							\
		zval *__z = (z);										\
		zend_array *_arr =										\
		(zend_array *) malloc(sizeof(zend_array));				\
		Z_ARR_P(__z) = _arr;									\
		Z_TYPE_INFO_P(__z) = IS_ARRAY_EX;						\
	} while (0)

//对象变量赋值
#define ZVAL_OBJ(z, o) do {						\
		zval *__z = (z);						\
		Z_OBJ_P(__z) = (o);						\
		Z_TYPE_INFO_P(__z) = IS_OBJECT_EX;		\
	} while (0)

//资源变量赋值
#define ZVAL_RES(z, r) do {						\
		zval *__z = (z);						\
		Z_RES_P(__z) = (r);						\
		Z_TYPE_INFO_P(__z) = IS_RESOURCE_EX;	\
	} while (0)

//新增资源变量
#define ZVAL_NEW_RES(z, h, p, t) do {							\
		zend_resource *_res =									\
		(zend_resource *) emalloc(sizeof(zend_resource));		\
		zval *__z;												\
		GC_REFCOUNT(_res) = 1;									\
		GC_TYPE_INFO(_res) = IS_RESOURCE;						\
		_res->handle = (h);										\
		_res->type = (t);										\
		_res->ptr = (p);										\
		__z = (z);												\
		Z_RES_P(__z) = _res;									\
		Z_TYPE_INFO_P(__z) = IS_RESOURCE_EX;					\
	} while (0)

//新增持久化资源变量
#define ZVAL_NEW_PERSISTENT_RES(z, h, p, t) do {				\
		zend_resource *_res =									\
		(zend_resource *) malloc(sizeof(zend_resource));		\
		zval *__z;												\
		GC_REFCOUNT(_res) = 1;									\
		GC_TYPE_INFO(_res) = IS_RESOURCE;						\
		_res->handle = (h);										\
		_res->type = (t);										\
		_res->ptr = (p);										\
		__z = (z);												\
		Z_RES_P(__z) = _res;									\
		Z_TYPE_INFO_P(__z) = IS_RESOURCE_EX;					\
	} while (0)

//引用变量赋值
#define ZVAL_REF(z, r) do {										\
		zval *__z = (z);										\
		Z_REF_P(__z) = (r);										\
		Z_TYPE_INFO_P(__z) = IS_REFERENCE_EX;					\
	} while (0)

//空引用赋值
#define ZVAL_NEW_EMPTY_REF(z) do {								\
		zend_reference *_ref =									\
		(zend_reference *) emalloc(sizeof(zend_reference));		\
		GC_REFCOUNT(_ref) = 1;									\
		GC_TYPE_INFO(_ref) = IS_REFERENCE;						\
		Z_REF_P(z) = _ref;										\
		Z_TYPE_INFO_P(z) = IS_REFERENCE_EX;						\
	} while (0)

//引用赋值
#define ZVAL_NEW_REF(z, r) do {									\
		zend_reference *_ref =									\
		(zend_reference *) emalloc(sizeof(zend_reference));		\
		GC_REFCOUNT(_ref) = 1;									\
		GC_TYPE_INFO(_ref) = IS_REFERENCE;						\
		ZVAL_COPY_VALUE(&_ref->val, r);							\
		Z_REF_P(z) = _ref;										\
		Z_TYPE_INFO_P(z) = IS_REFERENCE_EX;						\
	} while (0)

//持久化引用赋值
#define ZVAL_NEW_PERSISTENT_REF(z, r) do {						\
		zend_reference *_ref =									\
		(zend_reference *) malloc(sizeof(zend_reference));		\
		GC_REFCOUNT(_ref) = 1;									\
		GC_TYPE_INFO(_ref) = IS_REFERENCE;						\
		ZVAL_COPY_VALUE(&_ref->val, r);							\
		Z_REF_P(z) = _ref;										\
		Z_TYPE_INFO_P(z) = IS_REFERENCE_EX;						\
	} while (0)

//新建语法书
#define ZVAL_NEW_AST(z, a) do {									\
		zval *__z = (z);										\
		zend_ast_ref *_ast =									\
		(zend_ast_ref *) emalloc(sizeof(zend_ast_ref));			\
		GC_REFCOUNT(_ast) = 1;									\
		GC_TYPE_INFO(_ast) = IS_CONSTANT_AST;					\
		_ast->ast = (a);										\
		Z_AST_P(__z) = _ast;									\
		Z_TYPE_INFO_P(__z) = IS_CONSTANT_AST_EX;				\
	} while (0)

//设置变量间接值
#define ZVAL_INDIRECT(z, v) do {								\
		Z_INDIRECT_P(z) = (v);									\
		Z_TYPE_INFO_P(z) = IS_INDIRECT;							\
	} while (0)

//设置变量为字符串指针
#define ZVAL_PTR(z, p) do {										\
		Z_PTR_P(z) = (p);										\
		Z_TYPE_INFO_P(z) = IS_PTR;								\
	} while (0)

//设置变量为函数指针
#define ZVAL_FUNC(z, f) do {									\
		Z_FUNC_P(z) = (f);										\
		Z_TYPE_INFO_P(z) = IS_PTR;								\
	} while (0)

//设置变量为class指针
#define ZVAL_CE(z, c) do {										\
		Z_CE_P(z) = (c);										\
		Z_TYPE_INFO_P(z) = IS_PTR;								\
	} while (0)

#define ZVAL_ERROR(z) do {				\
		Z_TYPE_INFO_P(z) = _IS_ERROR;	\
	} while (0)

//指针类型变量
#define Z_REFCOUNT_P(pz)			zval_refcount_p(pz)	//获取计数
#define Z_SET_REFCOUNT_P(pz, rc)	zval_set_refcount_p(pz, rc) //设置计数
#define Z_ADDREF_P(pz)				zval_addref_p(pz)	//计数+
#define Z_DELREF_P(pz)				zval_delref_p(pz)	//计数-

//普通类型变量
#define Z_REFCOUNT(z)				Z_REFCOUNT_P(&(z)) //获取计数
#define Z_SET_REFCOUNT(z, rc)		Z_SET_REFCOUNT_P(&(z), rc) //设置计数
#define Z_ADDREF(z)					Z_ADDREF_P(&(z)) //计数+
#define Z_DELREF(z)					Z_DELREF_P(&(z))//计数-

//引用计数+1，判断是否支持
#define Z_TRY_ADDREF_P(pz) do {		\
	if (Z_REFCOUNTED_P((pz))) {		\
		Z_ADDREF_P((pz));			\
	}								\
} while (0)

//引用计数-1，判断是否支持
#define Z_TRY_DELREF_P(pz) do {		\
	if (Z_REFCOUNTED_P((pz))) {		\
		Z_DELREF_P((pz));			\
	}								\
} while (0)

#define Z_TRY_ADDREF(z)				Z_TRY_ADDREF_P(&(z))	//尝试引用计数+
#define Z_TRY_DELREF(z)				Z_TRY_DELREF_P(&(z))	//尝试引用计数-

//返回引用计数
static zend_always_inline uint32_t zval_refcount_p(zval* pz) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz) || Z_COPYABLE_P(pz));
	return GC_REFCOUNT(Z_COUNTED_P(pz));
}

//设置引用计数
static zend_always_inline uint32_t zval_set_refcount_p(zval* pz, uint32_t rc) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz));
	return GC_REFCOUNT(Z_COUNTED_P(pz)) = rc;
}

//增加引用计数
static zend_always_inline uint32_t zval_addref_p(zval* pz) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz));
	return ++GC_REFCOUNT(Z_COUNTED_P(pz));
}

//减少引用计数
static zend_always_inline uint32_t zval_delref_p(zval* pz) {
	ZEND_ASSERT(Z_REFCOUNTED_P(pz));
	return --GC_REFCOUNT(Z_COUNTED_P(pz));
}

#if SIZEOF_SIZE_T == 4
# define ZVAL_COPY_VALUE_EX(z, v, gc, t)				\
	do {												\
		uint32_t _w2 = v->value.ww.w2;					\
		Z_COUNTED_P(z) = gc;							\
		z->value.ww.w2 = _w2;							\
		Z_TYPE_INFO_P(z) = t;							\
	} while (0)
#elif SIZEOF_SIZE_T == 8
# define ZVAL_COPY_VALUE_EX(z, v, gc, t)				\
	do {												\
		Z_COUNTED_P(z) = gc;							\
		Z_TYPE_INFO_P(z) = t;							\
	} while (0)
#else
# error "Unknown SIZEOF_SIZE_T"
#endif

//变量复制值
#define ZVAL_COPY_VALUE(z, v)							\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);		\
		uint32_t _t = Z_TYPE_INFO_P(_z2);				\
		ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);			\
	} while (0)

//变量复制
#define ZVAL_COPY(z, v)									\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);		\
		uint32_t _t = Z_TYPE_INFO_P(_z2);				\
		ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);			\
		if ((_t & (IS_TYPE_REFCOUNTED << Z_TYPE_FLAGS_SHIFT)) != 0) { \
			GC_REFCOUNT(_gc)++;							\
		}												\
	} while (0)

//变量分离
#define ZVAL_DUP(z, v)									\
	do {												\
		zval *_z1 = (z);								\
		const zval *_z2 = (v);							\
		zend_refcounted *_gc = Z_COUNTED_P(_z2);		\
		uint32_t _t = Z_TYPE_INFO_P(_z2);				\
		ZVAL_COPY_VALUE_EX(_z1, _z2, _gc, _t);			\
		if ((_t & ((IS_TYPE_REFCOUNTED|IS_TYPE_COPYABLE) << Z_TYPE_FLAGS_SHIFT)) != 0) { \
			if ((_t & (IS_TYPE_COPYABLE << Z_TYPE_FLAGS_SHIFT)) != 0) { \
				_zval_copy_ctor_func(_z1 ZEND_FILE_LINE_CC); \
			} else {									\
				GC_REFCOUNT(_gc)++;						\
			}											\
		}												\
	} while (0)

//引用源
#define ZVAL_DEREF(z) do {								\
		if (UNEXPECTED(Z_ISREF_P(z))) {					\
			(z) = Z_REFVAL_P(z);						\
		}												\
	} while (0)

//操作引用
#define ZVAL_OPT_DEREF(z) do {							\
		if (UNEXPECTED(Z_OPT_ISREF_P(z))) {				\
			(z) = Z_REFVAL_P(z);						\
		}												\
	} while (0)

//新建引用
#define ZVAL_MAKE_REF(zv) do {							\
		zval *__zv = (zv);								\
		if (!Z_ISREF_P(__zv)) {							\
			ZVAL_NEW_REF(__zv, __zv);					\
		}												\
	} while (0)

//断开引用
#define ZVAL_UNREF(z) do {								\
		zval *_z = (z);									\
		zend_reference *ref;							\
		ZEND_ASSERT(Z_ISREF_P(_z));						\
		ref = Z_REF_P(_z);								\
		ZVAL_COPY_VALUE(_z, &ref->val);					\
		efree_size(ref, sizeof(zend_reference));		\
	} while (0)

//复制断开引用
#define ZVAL_COPY_UNREF(z, v) do {						\
		zval *_z3 = (v);								\
		if (Z_OPT_REFCOUNTED_P(_z3)) {					\
			if (UNEXPECTED(Z_OPT_ISREF_P(_z3))			\
			 && UNEXPECTED(Z_REFCOUNT_P(_z3) == 1)) {	\
				ZVAL_UNREF(_z3);						\
				if (Z_OPT_REFCOUNTED_P(_z3)) {			\
					Z_ADDREF_P(_z3);					\
				}										\
			} else {									\
				Z_ADDREF_P(_z3);						\
			}											\
		}												\
		ZVAL_COPY_VALUE(z, _z3);						\
	} while (0)

//分离字符串
#define SEPARATE_STRING(zv) do {						\
		zval *_zv = (zv);								\
		if (Z_REFCOUNT_P(_zv) > 1) {					\
			if (Z_REFCOUNTED_P(_zv)) {					\
				Z_DELREF_P(_zv);						\
			}											\
			zval_copy_ctor_func(_zv);					\
		}												\
	} while (0)

//分离数组
#define SEPARATE_ARRAY(zv) do {							\
		zval *_zv = (zv);								\
		zend_array *_arr = Z_ARR_P(_zv);				\
		if (UNEXPECTED(GC_REFCOUNT(_arr) > 1)) {		\
			if (Z_REFCOUNTED_P(_zv)) {					\
				GC_REFCOUNT(_arr)--;					\
			}											\
			ZVAL_ARR(_zv, zend_array_dup(_arr));		\
		}												\
	} while (0)

//分离非引用变量
#define SEPARATE_ZVAL_NOREF(zv) do {					\
		zval *_zv = (zv);								\
		ZEND_ASSERT(Z_TYPE_P(_zv) != IS_REFERENCE);		\
		if (Z_COPYABLE_P(_zv)) {						\
			if (Z_REFCOUNT_P(_zv) > 1) {				\
				if (Z_REFCOUNTED_P(_zv)) {				\
					Z_DELREF_P(_zv);					\
				}										\
				zval_copy_ctor_func(_zv);				\
			}											\
		}												\
	} while (0)

//分离zval
#define SEPARATE_ZVAL(zv) do {							\
		zval *_zv = (zv);								\
		if (Z_REFCOUNTED_P(_zv) ||						\
		    Z_COPYABLE_P(_zv)) {						\
			if (Z_REFCOUNT_P(_zv) > 1) {				\
				if (Z_COPYABLE_P(_zv)) {				\
					if (Z_REFCOUNTED_P(_zv)) {			\
						Z_DELREF_P(_zv);				\
					}									\
					zval_copy_ctor_func(_zv);			\
				} else if (Z_ISREF_P(_zv)) {			\
					Z_DELREF_P(_zv);					\
					ZVAL_DUP(_zv, Z_REFVAL_P(_zv));		\
				}										\
			}											\
		}												\
	} while (0)

//如果非引用，分离
#define SEPARATE_ZVAL_IF_NOT_REF(zv) do {				\
		zval *_zv = (zv);								\
		if (Z_COPYABLE_P(_zv)) {						\
			if (Z_REFCOUNT_P(_zv) > 1) {				\
				if (Z_REFCOUNTED_P(_zv)) {				\
					Z_DELREF_P(_zv);					\
				}										\
				zval_copy_ctor_func(_zv);				\
			}											\
		}												\
	} while (0)

#define SEPARATE_ARG_IF_REF(varptr) do { 				\
		ZVAL_DEREF(varptr);								\
		if (Z_REFCOUNTED_P(varptr)) { 					\
			Z_ADDREF_P(varptr); 						\
		}												\
	} while (0)

#endif /* ZEND_TYPES_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
