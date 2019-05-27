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
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_LIST_H
#define ZEND_LIST_H

#include "zend_hash.h"
#include "zend_globals.h"

BEGIN_EXTERN_C()

typedef void (*rsrc_dtor_func_t)(zend_resource *res);
#define ZEND_RSRC_DTOR_FUNC(name) void name(zend_resource *res)

//资源列表析构函数结构
typedef struct _zend_rsrc_list_dtors_entry {
	rsrc_dtor_func_t list_dtor_ex;   //普通列表析构函数
	rsrc_dtor_func_t plist_dtor_ex;  //持久化列表修购函数

	const char *type_name;  //资源类型名

	int module_number;   //模块ID
	int resource_id;  //资源ID(哈希表索引)
} zend_rsrc_list_dtors_entry;

//注册模块的列表析构函数
ZEND_API int zend_register_list_destructors_ex(rsrc_dtor_func_t ld, rsrc_dtor_func_t pld, const char *type_name, int module_number);

void list_entry_destructor(zval *ptr); //释放资源
void plist_entry_destructor(zval *ptr);   //释放持久化资源

void zend_clean_module_rsrc_dtors(int module_number); //清理模块的资源
int zend_init_rsrc_list(void);   //初始化普通资源列表
int zend_init_rsrc_plist(void);  //初始化持久化资源列表
void zend_close_rsrc_list(HashTable *ht); //关闭列表
void zend_destroy_rsrc_list(HashTable *ht);  //销毁链表
int zend_init_rsrc_list_dtors(void);   //初始化全局列表析构函数
void zend_destroy_rsrc_list_dtors(void);  //销毁全局列表析构函数

ZEND_API zval *zend_list_insert(void *ptr, int type); //插入资源指针到全局列表
ZEND_API int zend_list_free(zend_resource *res);   //从全局列表中释放一个资源
ZEND_API int zend_list_delete(zend_resource *res); //从全局列表中删除一个资源
ZEND_API int zend_list_close(zend_resource *res); //关闭一个资源

//注册一个资源到全局列表
ZEND_API zend_resource *zend_register_resource(void *rsrc_pointer, int rsrc_type); 
//获取一个资源的指针
ZEND_API void *zend_fetch_resource(zend_resource *res, const char *resource_type_name, int resource_type);
//获取一个资源的指针
ZEND_API void *zend_fetch_resource2(zend_resource *res, const char *resource_type_name, int resource_type, int resource_type2);
//获取变量中的资源指针
ZEND_API void *zend_fetch_resource_ex(zval *res, const char *resource_type_name, int resource_type);
//获取变量中的资源指针
ZEND_API void *zend_fetch_resource2_ex(zval *res, const char *resource_type_name, int resource_type, int resource_type2);

//获取资源的类型类型名称
ZEND_API const char *zend_rsrc_list_get_rsrc_type(zend_resource *res);
//通过类型名获取到资源ID（哈希表索引）
ZEND_API int zend_fetch_list_dtor_id(const char *type_name);

extern ZEND_API int le_index_ptr;  /* list entry type for index pointers */

END_EXTERN_C()

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
