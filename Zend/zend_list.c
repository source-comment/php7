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

/* resource lists */

#include "zend.h"
#include "zend_list.h"
#include "zend_API.h"
#include "zend_globals.h"

ZEND_API int le_index_ptr;

/* true global */
static HashTable list_destructors;

//这里的列表使用数值索引的哈希表实现的

/**
 * @description: 插入资源指针到全局列表
 * @param void* ptr 资源指针
 * @param int type 资源类型
 * @return: zval
 */
ZEND_API zval *zend_list_insert(void *ptr, int type)
{
	int index;
	zval zv;

	index = zend_hash_next_free_element(&EG(regular_list));
	if (index == 0) {
		index = 1;
	}
	ZVAL_NEW_RES(&zv, index, ptr, type);
	return zend_hash_index_add_new(&EG(regular_list), index, &zv);
}

/**
 * @description:  从全局列表中删除一个资源
 * @param zend_resource* res 待删除资源
 * @return: int
 */
ZEND_API int zend_list_delete(zend_resource *res)
{
	//引用计数减1后如果小于等于0，则直接从列表删除
	if (--GC_REFCOUNT(res) <= 0) { 
		return zend_hash_index_del(&EG(regular_list), res->handle);
	} else {
		return SUCCESS;
	}
}

/**
 * @description:  从全局列表中释放一个资源
 * @param zend_resource* res 待释放资源 
 * @return: int
 */
ZEND_API int zend_list_free(zend_resource *res)
{
	//如果引用计数小于等于0，则直接从列表删除
	if (GC_REFCOUNT(res) <= 0) {
		return zend_hash_index_del(&EG(regular_list), res->handle);
	} else {
		return SUCCESS;
	}
}

/**
 * @description: 资源释放
 * @param zend_resource* res 待操作资源 
 * @return: void
 */
static void zend_resource_dtor(zend_resource *res)
{
	zend_rsrc_list_dtors_entry *ld;
	zend_resource r = *res;

	res->type = -1;
	res->ptr = NULL;

	ld = zend_hash_index_find_ptr(&list_destructors, r.type);	//查找资源类型是否设置了析构函数
	if (ld) {
		if (ld->list_dtor_ex) {	//如果设置了析构函数，则调用析构函数
			ld->list_dtor_ex(&r);
		}
	} else {
		zend_error(E_WARNING, "Unknown list entry type (%d)", r.type);
	}
}

/**
 * @description: 关闭一个资源
 * @param zend_resource* res 待操作资源 
 * @return: int
 */
ZEND_API int zend_list_close(zend_resource *res)
{
	if (GC_REFCOUNT(res) <= 0) {
		return zend_list_free(res);	//如果引用计数小于等于0，则释放
	} else if (res->type >= 0) {
		zend_resource_dtor(res);	//如果资源类型大于0，则调用析构函数
	}
	return SUCCESS;
}

/**
 * @description: 注册一个资源到全局列表
 * @param zend_resource* rsrc_pointer 待注册资源 
 * @param int rsrc_type 资源类型
 * @return: zend_resource*
 */
ZEND_API zend_resource* zend_register_resource(void *rsrc_pointer, int rsrc_type)
{
	zval *zv;

	zv = zend_list_insert(rsrc_pointer, rsrc_type);

	return Z_RES_P(zv);
}

/**
 * @description: 获取一个资源的指针
 * @param zend_resource* res 待操作资源
 * @param char* resource_type_name 资源类型名（如果设置了，会获取前调用的类和方法，并且给出警告）
 * @param int resource_type1 资源的类型
 * @param int resource_type2 资源的类型
 * @return: void* 
 */
ZEND_API void *zend_fetch_resource2(zend_resource *res, const char *resource_type_name, int resource_type1, int resource_type2)
{
	if (res) {
		if (resource_type1 == res->type) {
			return res->ptr;
		}

		if (resource_type2 == res->type) {
			return res->ptr;
		}
	}

	if (resource_type_name) {
		const char *space;
		const char *class_name = get_active_class_name(&space);
		zend_error(E_WARNING, "%s%s%s(): supplied resource is not a valid %s resource", class_name, space, get_active_function_name(), resource_type_name);
	}

	return NULL;
}

/**
 * @description: 获取一个资源的指针
 * @param zend_resource* res 待操作资源
 * @param char* resource_type_name 资源类型名（如果设置了，会获取前调用的类和方法，并且给出警告）
 * @param int resource_type 资源的类型
 * @return: void* 
 */
ZEND_API void *zend_fetch_resource(zend_resource *res, const char *resource_type_name, int resource_type)
{
	if (resource_type == res->type) {
		return res->ptr;
	}

	if (resource_type_name) {
		const char *space;
		const char *class_name = get_active_class_name(&space);
		zend_error(E_WARNING, "%s%s%s(): supplied resource is not a valid %s resource", class_name, space, get_active_function_name(), resource_type_name);
	}

	return NULL;
}

/**
 * @description: 获取变量中的资源指针
 * @param zval* res 变量的指针
 * @param char* resource_type_name 资源类型名（如果设置了，会获取前调用的类和方法，并且给出警告）
 * @param int resource_type 资源的类型
 * @return: void* 
 */
ZEND_API void *zend_fetch_resource_ex(zval *res, const char *resource_type_name, int resource_type)
{
	const char *space, *class_name;
	//如果变量为NULL，则警告
	if (res == NULL) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): no %s resource supplied", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}

	//如果变量不是资源类型，则警告
	if (Z_TYPE_P(res) != IS_RESOURCE) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): supplied argument is not a valid %s resource", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}

	//返回变量的资源指针
	return zend_fetch_resource(Z_RES_P(res), resource_type_name, resource_type);
}

/**
 * @description: 获取变量中的资源指针
 * @param zval* res 变量的指针
 * @param char* resource_type_name 资源类型名（如果设置了，会获取前调用的类和方法，并且给出警告）
 * @param int resource_type1 资源的类型
 * @param int resource_type2 资源的类型
 * @return: void* 
 */
ZEND_API void *zend_fetch_resource2_ex(zval *res, const char *resource_type_name, int resource_type1, int resource_type2)
{
	const char *space, *class_name;
	//如果变量为NULL，则警告
	if (res == NULL) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): no %s resource supplied", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}

	//如果变量不是资源类型，则警告
	if (Z_TYPE_P(res) != IS_RESOURCE) {
		if (resource_type_name) {
			class_name = get_active_class_name(&space);
			zend_error(E_WARNING, "%s%s%s(): supplied argument is not a valid %s resource", class_name, space, get_active_function_name(), resource_type_name);
		}
		return NULL;
	}

	//返回变量的资源指针
	return zend_fetch_resource2(Z_RES_P(res), resource_type_name, resource_type1, resource_type2);
}

/**
 * @description: 释放资源变量
 * @param zval* zv 待操作变量
 * @return void
 */
void list_entry_destructor(zval *zv)
{
	zend_resource *res = Z_RES_P(zv);	//获取变量的资源数据

	ZVAL_UNDEF(zv);	//设为IS_UNDEF
	if (res->type >= 0) {
		zend_resource_dtor(res);	//调用销毁函数
	}
	efree_size(res, sizeof(zend_resource));	//释放内存
}

/**
 * @description: 释放持久化的资源变量
 * @param zval* zv 待操作变量
 * @return: void
 */
void plist_entry_destructor(zval *zv)
{
	zend_resource *res = Z_RES_P(zv);	//资源数据

	if (res->type >= 0) {
		zend_rsrc_list_dtors_entry *ld;

		ld = zend_hash_index_find_ptr(&list_destructors, res->type);	//检查是否设置资源类型的析构函数
		if (ld) {
			if (ld->plist_dtor_ex) {
				ld->plist_dtor_ex(res);	//调用析构函数释放资源
			}
		} else {
			zend_error(E_WARNING,"Unknown list entry type (%d)", res->type);
		}
	}

	//直接释放内存
	free(res);
}

//初始化资源列表（使用zend内存池）
int zend_init_rsrc_list(void)
{
	zend_hash_init(&EG(regular_list), 8, NULL, list_entry_destructor, 0);
	return SUCCESS;
}

//初始化持久资源列表，使用系统内存
int zend_init_rsrc_plist(void)
{
	zend_hash_init_ex(&EG(persistent_list), 8, NULL, plist_entry_destructor, 1, 0);
	return SUCCESS;
}

/**
 * @description: 关闭资源变量
 * @param zval* zv 待操作变量
 * @return: int 
 */
static int zend_close_rsrc(zval *zv)
{
	zend_resource *res = Z_PTR_P(zv);

	if (res->type >= 0) {
		zend_resource_dtor(res);
	}
	return ZEND_HASH_APPLY_KEEP;
}

/**
 * @description: 逆向顺序调用列表中的资源
 * @param HashTable* ht 待操作哈希表
 * @return: 
 */
void zend_close_rsrc_list(HashTable *ht)
{
	zend_hash_reverse_apply(ht, zend_close_rsrc);
}

/**
 * @description: 逆向顺序销毁列表
 * @param HashTable* ht 待操作哈希表
 * @return: void
 */
void zend_destroy_rsrc_list(HashTable *ht)
{
	zend_hash_graceful_reverse_destroy(ht);
}

/**
 * @description: 清理模块的资源
 * @param zval* 资源类型的变量
 * @param void* arg 资源类型
 * @return: int
 */
static int clean_module_resource(zval *zv, void *arg)
{
	int resource_id = *(int *)arg;
	if (Z_RES_TYPE_P(zv) == resource_id) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @description: 清理模块的资源
 * @param zval* zv 待操作变量
 * @param void* arg 资源类型
 * @return: 
 */
static int zend_clean_module_rsrc_dtors_cb(zval *zv, void *arg)
{
	zend_rsrc_list_dtors_entry *ld = (zend_rsrc_list_dtors_entry *)Z_PTR_P(zv);	//获取，回调函数变量
	int module_number = *(int *)arg;
	if (ld->module_number == module_number) {	//如果析构函数的模块id为模块指针，则销毁持久化列表中的资源
		zend_hash_apply_with_argument(&EG(persistent_list), clean_module_resource, (void *) &(ld->resource_id));
		return 1;
	} else {
		return 0;
	}
}

/**
 * @description: 销毁模块的资源析构函数
 * @param int module_number 模块编号
 * @return: void
 */
void zend_clean_module_rsrc_dtors(int module_number)
{
	zend_hash_apply_with_argument(&list_destructors, zend_clean_module_rsrc_dtors_cb, (void *) &module_number);
}

/**
 * @description: 注册模块的析构函数
 * @param rsrc_dtor_func_t* ld 列表析构函数 
 * @param rsrc_dtor_func_t* pld 持久化列表析构函数 
 * @param char* type_name 类型
 * @param int module_number 模块编号
 * @return: 
 */
ZEND_API int zend_register_list_destructors_ex(rsrc_dtor_func_t ld, rsrc_dtor_func_t pld, const char *type_name, int module_number)
{
	zend_rsrc_list_dtors_entry *lde;
	zval zv;

	lde = malloc(sizeof(zend_rsrc_list_dtors_entry));
	lde->list_dtor_ex = ld;
	lde->plist_dtor_ex = pld;
	lde->module_number = module_number;
	lde->resource_id = list_destructors.nNextFreeElement;
	lde->type_name = type_name;
	ZVAL_PTR(&zv, lde);

	//将析构函数表加入全局的列表析构函数表
	if (zend_hash_next_index_insert(&list_destructors, &zv) == NULL) {
		return FAILURE;
	}
	return list_destructors.nNextFreeElement-1;
}

/**
 * @description: 通过类型名获取到资源ID（哈希表索引）
 * @param char* type_name 类型名称
 * @return: int 析构方法的资源ID（哈表希索引）
 */
ZEND_API int zend_fetch_list_dtor_id(const char *type_name)
{
	zend_rsrc_list_dtors_entry *lde;

	ZEND_HASH_FOREACH_PTR(&list_destructors, lde) {
		if (lde->type_name && (strcmp(type_name, lde->type_name) == 0)) {
			return lde->resource_id;
		}
	} ZEND_HASH_FOREACH_END();

	return 0;
}

/**
 * @description: 释放析构函数列表
 * @param zval* 列表析构函数变量
 * @return: void
 */
static void list_destructors_dtor(zval *zv)
{
	free(Z_PTR_P(zv));
}

//初始化全局列表析构函数
int zend_init_rsrc_list_dtors(void)
{
	//全局列表析构函数指针，初始化大小。NULL，析构函数，1持久化哈希表
	zend_hash_init(&list_destructors, 64, NULL, list_destructors_dtor, 1);
	list_destructors.nNextFreeElement=1;	/* we don't want resource type 0 */
	return SUCCESS;
}

/**
 * @description: 销毁全局列表析构函数
 * @param zval* 列表析构函数变量
 * @return: void
 */
void zend_destroy_rsrc_list_dtors(void)
{
	zend_hash_destroy(&list_destructors);
}


/**
 * @description: 获取资源的类型类型名称
 * @param zend_resource* res 资源指针
 * @return: char*
 */
const char *zend_rsrc_list_get_rsrc_type(zend_resource *res)
{
	zend_rsrc_list_dtors_entry *lde;

	lde = zend_hash_index_find_ptr(&list_destructors, res->type);
	if (lde) {
		return lde->type_name;
	} else {
		return NULL;
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
