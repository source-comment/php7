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

#ifndef ZEND_LLIST_H
#define ZEND_LLIST_H

//双向链表元素结构体
typedef struct _zend_llist_element {
	struct _zend_llist_element *next;   //上一个
	struct _zend_llist_element *prev;   //下一个
	char data[1]; /* Needs to always be last in the struct */   //数据
} zend_llist_element;

typedef void (*llist_dtor_func_t)(void *); //链表析构函数
typedef int (*llist_compare_func_t)(const zend_llist_element **, const zend_llist_element **); //链表元素对比函数
typedef void (*llist_apply_with_args_func_t)(void *data, int num_args, va_list args);  //多个参数的回调函数
typedef void (*llist_apply_with_arg_func_t)(void *data, void *arg);  //一个参数的回调函数
typedef void (*llist_apply_func_t)(void *);  //无参数的回调函数

//链表管理结构体
typedef struct _zend_llist {
	zend_llist_element *head; //链表头
	zend_llist_element *tail;  //链表尾
	size_t count;  //元素数量
	size_t size;   //内存大小
	llist_dtor_func_t dtor; //析构函数
	unsigned char persistent;  //是否持久化
	zend_llist_element *traverse_ptr;   
} zend_llist;

typedef zend_llist_element* zend_llist_position;

BEGIN_EXTERN_C()
//初始化双向链表
ZEND_API void zend_llist_init(zend_llist *l, size_t size, llist_dtor_func_t dtor, unsigned char persistent);
//添加元素到链表尾部
ZEND_API void zend_llist_add_element(zend_llist *l, void *element);
//添加元素到链表头部
ZEND_API void zend_llist_prepend_element(zend_llist *l, void *element);
//删除链表中元素
ZEND_API void zend_llist_del_element(zend_llist *l, void *element, int (*compare)(void *element1, void *element2));
//销毁链表
ZEND_API void zend_llist_destroy(zend_llist *l);
//清理链表
ZEND_API void zend_llist_clean(zend_llist *l);
//移除链表尾部元素
ZEND_API void zend_llist_remove_tail(zend_llist *l);
//链表复制
ZEND_API void zend_llist_copy(zend_llist *dst, zend_llist *src);
//对链表元素逐个执行函数
ZEND_API void zend_llist_apply(zend_llist *l, llist_apply_func_t func);
//通过回调函数返回值删除链表中元素
ZEND_API void zend_llist_apply_with_del(zend_llist *l, int (*func)(void *data));
//对链表中元素逐个执行有一个参数的函数
ZEND_API void zend_llist_apply_with_argument(zend_llist *l, llist_apply_with_arg_func_t func, void *arg);
//对链表中元素逐个执行有多个参数的函数
ZEND_API void zend_llist_apply_with_arguments(zend_llist *l, llist_apply_with_args_func_t func, int num_args, ...);
//获取链表中元素数量
ZEND_API size_t zend_llist_count(zend_llist *l);
//链表排序
ZEND_API void zend_llist_sort(zend_llist *l, llist_compare_func_t comp_func);

/* traversal */
ZEND_API void *zend_llist_get_first_ex(zend_llist *l, zend_llist_position *pos); //获取第一个元素
ZEND_API void *zend_llist_get_last_ex(zend_llist *l, zend_llist_position *pos);  //获取最后一个元素
ZEND_API void *zend_llist_get_next_ex(zend_llist *l, zend_llist_position *pos);  //获取下一个元素
ZEND_API void *zend_llist_get_prev_ex(zend_llist *l, zend_llist_position *pos);  //获取上一个元素

//获取第一个元素
#define zend_llist_get_first(l) zend_llist_get_first_ex(l, NULL)
//获取最后一个元素
#define zend_llist_get_last(l) zend_llist_get_last_ex(l, NULL)
//获取下一个元素
#define zend_llist_get_next(l) zend_llist_get_next_ex(l, NULL)
//获取上一个元素
#define zend_llist_get_prev(l) zend_llist_get_prev_ex(l, NULL)

END_EXTERN_C()

#endif /* ZEND_LLIST_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
