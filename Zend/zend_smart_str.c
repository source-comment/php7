/*
   +----------------------------------------------------------------------+
   | PHP Version 7                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2018 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Dmitry Stogov <dmitry@zend.com>                              |
   +----------------------------------------------------------------------+
 */

#include <zend.h>
#include "zend_smart_str.h"

#define SMART_STR_OVERHEAD (ZEND_MM_OVERHEAD + _ZSTR_HEADER_SIZE)

#ifndef SMART_STR_PAGE
# define SMART_STR_PAGE 4096
#endif

#ifndef SMART_STR_START_SIZE
# define SMART_STR_START_SIZE (256 - SMART_STR_OVERHEAD - 1)
#endif

#define SMART_STR_NEW_SIZE(len) \
	(((len + SMART_STR_OVERHEAD + SMART_STR_PAGE) & ~(SMART_STR_PAGE - 1)) - SMART_STR_OVERHEAD - 1)

/**
 * @description: 向内存池为smart_str申请内存
 * @param smart_str* str 指针
 * @param size_t len 长度
 * @return: void
 */
ZEND_API void ZEND_FASTCALL smart_str_erealloc(smart_str *str, size_t len)
{
	if (UNEXPECTED(!str->s)) {	
		//实际值为空，则新申请内存
		str->a = len < SMART_STR_START_SIZE
				? SMART_STR_START_SIZE
				: SMART_STR_NEW_SIZE(len);
		str->s = zend_string_alloc(str->a, 0);
		ZSTR_LEN(str->s) = 0;
	} else {
		//如果已经有值，则扩展内存
		str->a = SMART_STR_NEW_SIZE(len);
		str->s = (zend_string *) erealloc2(str->s, _ZSTR_HEADER_SIZE + str->a + 1, _ZSTR_HEADER_SIZE + ZSTR_LEN(str->s) + 1);
	}
}

/**
 * @description: 向系统为smart_str申请内存
 * @param smart_str* str 指针
 * @param size_t len 长度
 * @return: void
 */
ZEND_API void ZEND_FASTCALL smart_str_realloc(smart_str *str, size_t len)
{
	if (UNEXPECTED(!str->s)) {
		//实际值为空，则新申请内存
		str->a = len < SMART_STR_START_SIZE
				? SMART_STR_START_SIZE
				: SMART_STR_NEW_SIZE(len);
		str->s = zend_string_alloc(str->a, 1);
		ZSTR_LEN(str->s) = 0;
	} else {
		//如果已经有值，则扩展内存
		str->a = SMART_STR_NEW_SIZE(len);
		str->s = (zend_string *) realloc(str->s, _ZSTR_HEADER_SIZE + str->a + 1);
	}
}

/* Windows uses VK_ESCAPE instead of \e */
#ifndef VK_ESCAPE
#define VK_ESCAPE '\e'
#endif

/**
 * @description: 计算字符串的长度，包含\r,\n,\f,\t,\v,\\,\e,以及ASCII表中的特殊字符
 * @param char* s 字符串
 * @param size_t l 字符串本身长度，不包含转移和特殊字符
 * @return: size_t
 */
static size_t zend_compute_escaped_string_len(const char *s, size_t l) {
	size_t i, len = l;
	for (i = 0; i < l; ++i) {
		char c = s[i];
		if (c == '\n' || c == '\r' || c == '\t' ||
			c == '\f' || c == '\v' || c == '\\' || c == VK_ESCAPE) {
			len += 1;	//转移字符1个长度
		} else if (c < 32 || c > 126) {
			//特殊字符占用3长度
			len += 3;
		}
	}
	return len;
}

/**
 * @description: 追加特殊字或转移字符到smart_str末尾
 * @param smart_str * str smart_str指针地址
 * @param char* s 追加字符串
 * @param size_t l 追加字符串长度
 * @return: void
 */
ZEND_API void ZEND_FASTCALL smart_str_append_escaped(smart_str *str, const char *s, size_t l) {
	char *res;
	size_t i, len = zend_compute_escaped_string_len(s, l);

	smart_str_alloc(str, len, 0);
	res = &ZSTR_VAL(str->s)[ZSTR_LEN(str->s)];
	ZSTR_LEN(str->s) += len;

	for (i = 0; i < l; ++i) {
		unsigned char c = s[i];
		if (c < 32 || c == '\\' || c > 126) {
			*res++ = '\\';
			switch (c) {
				case '\n': *res++ = 'n'; break;
				case '\r': *res++ = 'r'; break;
				case '\t': *res++ = 't'; break;
				case '\f': *res++ = 'f'; break;
				case '\v': *res++ = 'v'; break;
				case '\\': *res++ = '\\'; break;
				case VK_ESCAPE: *res++ = 'e'; break;
				default:
					*res++ = 'x';
					if ((c >> 4) < 10) {
						*res++ = (c >> 4) + '0';
					} else {
						*res++ = (c >> 4) + 'A' - 10;
					}
					if ((c & 0xf) < 10) {
						*res++ = (c & 0xf) + '0';
					} else {
						*res++ = (c & 0xf) + 'A' - 10;
					}
			}
		} else {
			*res++ = c;
		}
	}
}

/**
 * @description: 打印smart_str
 * @param smart_str* dest smart_str指针
 * @param char* format 格式化
 * @param ....参数列表
 * @return: 
 */
ZEND_API void ZEND_FASTCALL smart_str_append_printf(smart_str *dest, const char *format, ...) {
	va_list arg;
	va_start(arg, format);
	zend_printf_to_smart_str(dest, format, arg);
	va_end(arg);
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
