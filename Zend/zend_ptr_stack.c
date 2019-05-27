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

#include "zend.h"
#include "zend_ptr_stack.h"
#ifdef HAVE_STDARG_H
# include <stdarg.h>
#endif

/**
 * @description: 指针栈初始化
 * @param zend_ptr_stack* stack 栈首地址
 * @param zend_bool persistent 是否持久化
 * @return: 
 */
ZEND_API void zend_ptr_stack_init_ex(zend_ptr_stack *stack, zend_bool persistent)
{
	stack->top_element = stack->elements = NULL;
	stack->top = stack->max = 0;
	stack->persistent = persistent;
}

/**
 * @description: 指针栈初始化
 * @param zend_ptr_stack* stack 栈首地址
 * @return: void
 */
ZEND_API void zend_ptr_stack_init(zend_ptr_stack *stack)
{
	zend_ptr_stack_init_ex(stack, 0);
}

/**
 * @description: 将多个元素入栈
 * @param zend_ptr_stack* stack 栈首地址
 * param int count 元素数量
 * @param ... 元素列表
 * @return: void
 */
ZEND_API void zend_ptr_stack_n_push(zend_ptr_stack *stack, int count, ...)
{
	va_list ptr;
	void *elem;

	ZEND_PTR_STACK_RESIZE_IF_NEEDED(stack, count)

	va_start(ptr, count);
	while (count>0) {
		elem = va_arg(ptr, void *);
		stack->top++;
		*(stack->top_element++) = elem;
		count--;
	}
	va_end(ptr);
}

/**
 * @description: 将多个元素出栈
 * @param zend_ptr_stack* stack 栈首地址
 * param int count 元素数量
 * @return: void
 */
ZEND_API void zend_ptr_stack_n_pop(zend_ptr_stack *stack, int count, ...)
{
	va_list ptr;
	void **elem;

	va_start(ptr, count);
	while (count>0) {
		elem = va_arg(ptr, void **);
		*elem = *(--stack->top_element);
		stack->top--;
		count--;
	}
	va_end(ptr);
}

/**
 * @description: 销毁指针栈
 * @param zend_ptr_stack* stack 栈首地址
 * @return: void
 */
ZEND_API void zend_ptr_stack_destroy(zend_ptr_stack *stack)
{
	if (stack->elements) {
		pefree(stack->elements, stack->persistent);
	}
}

/**
 * @description: 对栈内元素执行函数
 * @param zend_ptr_stack* stack 栈首地址
 * @param void(*func) 函数
 * @return: void
 */
ZEND_API void zend_ptr_stack_apply(zend_ptr_stack *stack, void (*func)(void *))
{
	int i = stack->top;

	while (--i >= 0) {
		func(stack->elements[i]);
	}
}

/**
 * @description: 对栈内元素执行函数，并清理
 * @param zend_ptr_stack* stack 栈首地址
 * @param void(*func) 函数
 * @param zend_bool free_elements 是否释放元素内存
 * @return: void
 */
ZEND_API void zend_ptr_stack_clean(zend_ptr_stack *stack, void (*func)(void *), zend_bool free_elements)
{
	zend_ptr_stack_apply(stack, func);	//遍历栈并逐个对元素执行函数
	if (free_elements) {
		int i = stack->top;

		while (--i >= 0) {
			pefree(stack->elements[i], stack->persistent);
		}
	}
	stack->top = 0;
	stack->top_element = stack->elements;
}

/**
 * @description: 获取栈的元素数量
 * @param zend_ptr_stack* stack 栈首地址
 * @return: int
 */
ZEND_API int zend_ptr_stack_num_elements(zend_ptr_stack *stack)
{
	return stack->top;
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
