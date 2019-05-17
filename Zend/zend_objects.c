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
#include "zend_interfaces.h"
#include "zend_exceptions.h"

/**
 * @description:  对象初始化
 * @param zend_object* object 对象指针
 * @param zend_class_entry* ce 类指针
 * @return: void
 */
ZEND_API void zend_object_std_init(zend_object *object, zend_class_entry *ce)
{
	GC_REFCOUNT(object) = 1;	//设置gc.refcount = 1
	GC_TYPE_INFO(object) = IS_OBJECT | (GC_COLLECTABLE << GC_FLAGS_SHIFT);	//标记属性为对象并且可回收
	object->ce = ce;	//设置对象的class
	object->properties = NULL;	//设置成员属性表为NULL
	zend_objects_store_put(object);	//将对象增加至全局对象仓库
	if (UNEXPECTED(ce->ce_flags & ZEND_ACC_USE_GUARDS)) {
		GC_FLAGS(object) |= IS_OBJ_USE_GUARDS;	//魔术方法递归保护
		//将成员属性表中，默认属性数之后的一个属性位置设为IS_UNDEF
		ZVAL_UNDEF(object->properties_table + object->ce->default_properties_count); 
	}
}

/**
 * @description: 对象清理函数
 * @param zend_object* object 待操作对象
 * @return: void
 */
ZEND_API void zend_object_std_dtor(zend_object *object)
{
	zval *p, *end;

	if (object->properties) {
		if (EXPECTED(!(GC_FLAGS(object->properties) & IS_ARRAY_IMMUTABLE))) { //非固定数组
			if (EXPECTED(--GC_REFCOUNT(object->properties) == 0)) {
				zend_array_destroy(object->properties);	//如果成员属性表计数为0，直接销毁
			}
		}
	}
	p = object->properties_table; //属性表
	if (EXPECTED(object->ce->default_properties_count)) { //默认成员属性数量大于0
		end = p + object->ce->default_properties_count; //尾部
		do {
			i_zval_ptr_dtor(p ZEND_FILE_LINE_CC);		//回收对象成员属性
			p++;
		} while (p != end);
	}
	if (UNEXPECTED(GC_FLAGS(object) & IS_OBJ_HAS_GUARDS)) {
		//如果属性表类型为字符串则直接释放
		if (EXPECTED(Z_TYPE_P(p) == IS_STRING)) {	
			zend_string_release(Z_STR_P(p));
		} else {
			HashTable *guards;
			//释放成员属性哈希表
			ZEND_ASSERT(Z_TYPE_P(p) == IS_ARRAY);
			guards = Z_ARRVAL_P(p);
			ZEND_ASSERT(guards != NULL);
			zend_hash_destroy(guards);
			FREE_HASHTABLE(guards);
		}
	}
}

/**
 * @description: 对象销毁函数
 * @param zend_object* object 待操作对象
 * @return: void
 */
ZEND_API void zend_objects_destroy_object(zend_object *object)
{
	//获取析构函数
	zend_function *destructor = object->ce->destructor;

	//如果析构函数已经设置
	if (destructor) {
		zend_object *old_exception;
		zval obj;
		zend_class_entry *orig_fake_scope;

		//如果析构函数是private和protected
		if (destructor->op_array.fn_flags & (ZEND_ACC_PRIVATE|ZEND_ACC_PROTECTED)) {
			//如果析构函数是private的
			if (destructor->op_array.fn_flags & ZEND_ACC_PRIVATE) {
				/* Ensure that if we're calling a private function, we're allowed to do so.
				 */
				if (EG(current_execute_data)) {
					zend_class_entry *scope = zend_get_executed_scope();

					if (object->ce != scope) {
						zend_throw_error(NULL,
							"Call to private %s::__destruct() from context '%s'",
							ZSTR_VAL(object->ce->name),
							scope ? ZSTR_VAL(scope->name) : "");
						return;
					}
				} else {
					zend_error(E_WARNING,
						"Call to private %s::__destruct() from context '' during shutdown ignored",
						ZSTR_VAL(object->ce->name));
					return;
				}
			} else {
				/* Ensure that if we're calling a protected function, we're allowed to do so.
				 */
				if (EG(current_execute_data)) {
					zend_class_entry *scope = zend_get_executed_scope();

					if (!zend_check_protected(zend_get_function_root_class(destructor), scope)) {
						zend_throw_error(NULL,
							"Call to protected %s::__destruct() from context '%s'",
							ZSTR_VAL(object->ce->name),
							scope ? ZSTR_VAL(scope->name) : "");
						return;
					}
				} else {
					zend_error(E_WARNING,
						"Call to protected %s::__destruct() from context '' during shutdown ignored",
						ZSTR_VAL(object->ce->name));
					return;
				}
			}
		}

		GC_REFCOUNT(object)++;
		ZVAL_OBJ(&obj, object);

		/* Make sure that destructors are protected from previously thrown exceptions.
		 * For example, if an exception was thrown in a function and when the function's
		 * local variable destruction results in a destructor being called.
		 */
		old_exception = NULL;
		if (EG(exception)) {
			if (EG(exception) == object) {
				zend_error_noreturn(E_CORE_ERROR, "Attempt to destruct pending exception");
			} else {
				old_exception = EG(exception);
				EG(exception) = NULL;
			}
		}
		orig_fake_scope = EG(fake_scope);
		EG(fake_scope) = NULL;
		zend_call_method_with_0_params(&obj, object->ce, &destructor, ZEND_DESTRUCTOR_FUNC_NAME, NULL);
		if (old_exception) {
			if (EG(exception)) {
				zend_exception_set_previous(EG(exception), old_exception);
			} else {
				EG(exception) = old_exception;
			}
		}
		zval_ptr_dtor(&obj);
		EG(fake_scope) = orig_fake_scope;
	}
}

/**
 * @description: 创建对象
 * @param zend_class_entry* ce 类指针
 * @return: zend_object 对象
 */
ZEND_API zend_object *zend_objects_new(zend_class_entry *ce)
{
	zend_object *object = emalloc(sizeof(zend_object) + zend_object_properties_size(ce));

	zend_object_std_init(object, ce);
	object->handlers = &std_object_handlers;
	return object;
}

/**
 * @description: 克隆对象的成员到新对象
 * @param zend_object* new_object
 * @param zend_object* old_object
 * @return: void
 */
ZEND_API void zend_objects_clone_members(zend_object *new_object, zend_object *old_object)
{
	//克隆旧对象默认属性表
	if (old_object->ce->default_properties_count) {
		zval *src = old_object->properties_table;
		zval *dst = new_object->properties_table;
		zval *end = src + old_object->ce->default_properties_count;

		do {
			i_zval_ptr_dtor(dst ZEND_FILE_LINE_CC);	//销毁dst
			ZVAL_COPY_VALUE(dst, src);	//复制src到dst
			zval_add_ref(dst); //dst计数+
			src++;	//下一个
			dst++;	//下一个
		} while (src != end);
	} else if (old_object->properties && !old_object->ce->clone) {
		//如果旧对象属性表不为空，并且已经设置克隆函数
		/* fast copy */
		if (EXPECTED(old_object->handlers == &std_object_handlers)) {
			//如果旧对象的属性表非固定数组，则引用计数增加，并赋值给新对象
			if (EXPECTED(!(GC_FLAGS(old_object->properties) & IS_ARRAY_IMMUTABLE))) {
				GC_REFCOUNT(old_object->properties)++;
			}
			new_object->properties = old_object->properties;
			return;
		}
	}

	//如果旧对象属性表不为空，并且有效成员大于0
	if (old_object->properties &&
	    EXPECTED(zend_hash_num_elements(old_object->properties))) {
		zval *prop, new_prop;
		zend_ulong num_key;
		zend_string *key;

		if (!new_object->properties) {
			//如果新对象成员属性表为空，则新申请内存
			ALLOC_HASHTABLE(new_object->properties);
			//初始化旧对象属性表有效元素个数的属性表元素
			zend_hash_init(new_object->properties, zend_hash_num_elements(old_object->properties), NULL, ZVAL_PTR_DTOR, 0);
			//初始化属性表为枚举哈希表，索引区都是-1
			zend_hash_real_init(new_object->properties, 0);
		} else {
			//如果新对象属性表不为空，则扩展新对象的属性表至新对象属性数量+旧对象有效属性数
			zend_hash_extend(new_object->properties, new_object->properties->nNumUsed + zend_hash_num_elements(old_object->properties), 0);
		}

		//设置新对象属性表的属性
		new_object->properties->u.v.flags |=
			old_object->properties->u.v.flags & HASH_FLAG_HAS_EMPTY_IND;

		ZEND_HASH_FOREACH_KEY_VAL(old_object->properties, num_key, key, prop) {
			//如果是间接类型类型，则复制给new_prop
			if (Z_TYPE_P(prop) == IS_INDIRECT) {
				ZVAL_INDIRECT(&new_prop, new_object->properties_table + (Z_INDIRECT_P(prop) - old_object->properties_table));
			} else {
				//复制给new_prop,引用计数加1
				ZVAL_COPY_VALUE(&new_prop, prop);
				zval_add_ref(&new_prop);
			}
			//加入属性到新对象的属性表，
			if (EXPECTED(key)) {
				_zend_hash_append(new_object->properties, key, &new_prop);
			} else {
				zend_hash_index_add_new(new_object->properties, num_key, &new_prop);
			}
		} ZEND_HASH_FOREACH_END();
	}

	if (old_object->ce->clone) {
		//如果旧对象实现了克隆方法，则调用克隆方法生成新对象
		zval new_obj;

		ZVAL_OBJ(&new_obj, new_object);
		Z_ADDREF(new_obj);
		zend_call_method_with_0_params(&new_obj, old_object->ce, &old_object->ce->clone, ZEND_CLONE_FUNC_NAME, NULL);
		zval_ptr_dtor(&new_obj);
	}
}

/**
 * @description: 克隆对象
 * @param zval* zobject 需要克隆的对象
 * @return: zval* 克隆出来的对象
 */
ZEND_API zend_object *zend_objects_clone_obj(zval *zobject)
{
	zend_object *old_object;
	zend_object *new_object;

	/* assume that create isn't overwritten, so when clone depends on the
	 * overwritten one then it must itself be overwritten */
	old_object = Z_OBJ_P(zobject);
	new_object = zend_objects_new(old_object->ce);

	/* zend_objects_clone_members() expect the properties to be initialized. */
	if (new_object->ce->default_properties_count) {
		zval *p = new_object->properties_table;
		zval *end = p + new_object->ce->default_properties_count;
		do {
			ZVAL_UNDEF(p);
			p++;
		} while (p != end);
	}

	zend_objects_clone_members(new_object, old_object);

	return new_object;
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
