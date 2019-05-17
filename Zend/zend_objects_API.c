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
#include "zend_API.h"
#include "zend_objects_API.h"

/**
 * @description: 初始化对象仓库
 * @param zend_objects_store*  objects 对象仓库指
 * @param uint32_t init_size 仓库数量
 * @return: 
 */
ZEND_API void zend_objects_store_init(zend_objects_store *objects, uint32_t init_size)
{
	objects->object_buckets = (zend_object **) emalloc(init_size * sizeof(zend_object*));	//申请内存
	objects->top = 1; /* Skip 0 so that handles are true */
	objects->size = init_size;
	objects->free_list_head = -1;
	memset(&objects->object_buckets[0], 0, sizeof(zend_object*));
}

/**
 * @description: 销毁对象仓库
 * @param zend_objects_store*  objects 对象仓库
 * @return: void
 */
ZEND_API void zend_objects_store_destroy(zend_objects_store *objects)
{
	efree(objects->object_buckets);	//释放内存
	objects->object_buckets = NULL;	 //设为NULL
}

/**
 * @description: 对象仓库遍历调用析构函数
 * @param zend_objects_store* objects 对象仓库
 * @return: void
 */
ZEND_API void zend_objects_store_call_destructors(zend_objects_store *objects)
{
	EG(flags) |= EG_FLAGS_OBJECT_STORE_NO_REUSE;
	if (objects->top > 1) {
		uint32_t i;
		for (i = 1; i < objects->top; i++) {
			zend_object *obj = objects->object_buckets[i]; //获取对象
			if (IS_OBJ_VALID(obj)) {
				if (!(GC_FLAGS(obj) & IS_OBJ_DESTRUCTOR_CALLED)) {	//析构函数未调用
					GC_FLAGS(obj) |= IS_OBJ_DESTRUCTOR_CALLED;	//设为已调用

					if (obj->handlers->dtor_obj
					 && (obj->handlers->dtor_obj != zend_objects_destroy_object
					  || obj->ce->destructor)) {
						GC_REFCOUNT(obj)++;	//计数+
						obj->handlers->dtor_obj(obj);	//调用析构函数
						GC_REFCOUNT(obj)--;	//计数-
					}
				}
			}
		}
	}
}

/**
 * @description: 对象仓库遍历标记为已调用析构函数
 * @param zend_objects_store* objects 对象仓库
 * @return: void
 */
ZEND_API void zend_objects_store_mark_destructed(zend_objects_store *objects)
{
	if (objects->object_buckets && objects->top > 1) {
		zend_object **obj_ptr = objects->object_buckets + 1;
		zend_object **end = objects->object_buckets + objects->top;

		do {
			zend_object *obj = *obj_ptr;

			if (IS_OBJ_VALID(obj)) {
				GC_FLAGS(obj) |= IS_OBJ_DESTRUCTOR_CALLED;
			}
			obj_ptr++;
		} while (obj_ptr != end);
	}
}

/**
 * @description: 释放对象仓库的对象
 * @param zend_objects_store* objects 对象仓库
 * @param zend_bol fast_shutdown 是否快速
 * @return: 
 */
ZEND_API void zend_objects_store_free_object_storage(zend_objects_store *objects, zend_bool fast_shutdown)
{
	zend_object **obj_ptr, **end, *obj;

	if (objects->top <= 1) {
		return;
	}

	/* Free object contents, but don't free objects themselves, so they show up as leaks */
	end = objects->object_buckets + 1;
	obj_ptr = objects->object_buckets + objects->top;

	if (fast_shutdown) {
		do {
			obj_ptr--;
			obj = *obj_ptr;
			if (IS_OBJ_VALID(obj)) {
				if (!(GC_FLAGS(obj) & IS_OBJ_FREE_CALLED)) {	//清理函数未调用
					GC_FLAGS(obj) |= IS_OBJ_FREE_CALLED;	//标记为已调用清理函数
					if (obj->handlers->free_obj && obj->handlers->free_obj != zend_object_std_dtor) {
						GC_REFCOUNT(obj)++;
						obj->handlers->free_obj(obj);	//调用清理函数
						GC_REFCOUNT(obj)--;
					}
				}
			}
		} while (obj_ptr != end);
	} else {
		do {
			obj_ptr--;
			obj = *obj_ptr;
			if (IS_OBJ_VALID(obj)) {
				if (!(GC_FLAGS(obj) & IS_OBJ_FREE_CALLED)) {	//清理函数未调用
					GC_FLAGS(obj) |= IS_OBJ_FREE_CALLED; //标记为已调用清理函数
					if (obj->handlers->free_obj) {
						GC_REFCOUNT(obj)++;
						obj->handlers->free_obj(obj);//调用清理函数
						GC_REFCOUNT(obj)--;
					}
				}
			}
		} while (obj_ptr != end);
	}
}


/* Store objects API */
/**
 * @description: 新增一个对象到对象仓库
 * @param zend_object* object
 * @return: void
 */
ZEND_API void zend_objects_store_put(zend_object *object)
{
	int handle;

	/* When in shutdown sequence - do not reuse previously freed handles, to make sure
	 * the dtors for newly created objects are called in zend_objects_store_call_destructors() loop
	 */
	if (EG(objects_store).free_list_head != -1 && EXPECTED(!(EG(flags) & EG_FLAGS_OBJECT_STORE_NO_REUSE))) {
		handle = EG(objects_store).free_list_head;
		EG(objects_store).free_list_head = GET_OBJ_BUCKET_NUMBER(EG(objects_store).object_buckets[handle]);
	} else {
		//如果没有空闲空间，则扩容对象仓库为原来的2倍大小
		if (EG(objects_store).top == EG(objects_store).size) {
			uint32_t new_size = 2 * EG(objects_store).size;
			EG(objects_store).object_buckets = (zend_object **) erealloc(EG(objects_store).object_buckets, new_size * sizeof(zend_object*));
			/* Assign size after realloc, in case it fails */
			EG(objects_store).size = new_size;
		}
		//handle指向新对象仓库第一个空闲位置
		handle = EG(objects_store).top++;
	}
	object->handle = handle;
	EG(objects_store).object_buckets[handle] = object;
}

#define ZEND_OBJECTS_STORE_ADD_TO_FREE_LIST(handle)															\
            SET_OBJ_BUCKET_NUMBER(EG(objects_store).object_buckets[handle], EG(objects_store).free_list_head);	\
			EG(objects_store).free_list_head = handle;

/**
 * @description: 从对象仓库删除一个对象
 * @param zend_object* object 待删除对象
 * @return: void
 */
ZEND_API void zend_objects_store_del(zend_object *object) /* {{{ */
{
	/*	Make sure we hold a reference count during the destructor call
		otherwise, when the destructor ends the storage might be freed
		when the refcount reaches 0 a second time
	 */
	if (EG(objects_store).object_buckets &&
	    IS_OBJ_VALID(EG(objects_store).object_buckets[object->handle])) {
		if (GC_REFCOUNT(object) == 0) {
			if (!(GC_FLAGS(object) & IS_OBJ_DESTRUCTOR_CALLED)) {
				GC_FLAGS(object) |= IS_OBJ_DESTRUCTOR_CALLED;

				if (object->handlers->dtor_obj
				 && (object->handlers->dtor_obj != zend_objects_destroy_object
				  || object->ce->destructor)) {
					GC_REFCOUNT(object)++;
					object->handlers->dtor_obj(object);
					GC_REFCOUNT(object)--;
				}
			}

			if (GC_REFCOUNT(object) == 0) {
				uint32_t handle = object->handle;
				void *ptr;

				EG(objects_store).object_buckets[handle] = SET_OBJ_INVALID(object);
				if (!(GC_FLAGS(object) & IS_OBJ_FREE_CALLED)) {
					GC_FLAGS(object) |= IS_OBJ_FREE_CALLED;
					if (object->handlers->free_obj) {
						GC_REFCOUNT(object)++;
						object->handlers->free_obj(object);
						GC_REFCOUNT(object)--;
					}
				}
				ptr = ((char*)object) - object->handlers->offset;
				GC_REMOVE_FROM_BUFFER(object);
				efree(ptr);
				ZEND_OBJECTS_STORE_ADD_TO_FREE_LIST(handle);
			}
		} else {
			GC_REFCOUNT(object)--;
		}
	}
}
/* }}} */

ZEND_API zend_object_handlers *zend_get_std_object_handlers(void)
{
	return &std_object_handlers;
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
