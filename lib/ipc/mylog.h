#pragma once

#if defined (__cplusplus)
extern "C" {
#endif
void mylog(char c, char *str);
#if defined (__cplusplus)
}
#endif

#ifndef MOD
#define MOD ""
#endif
#define LOG(c,...) \
{\
	char _str_[1024];\
	snprintf(_str_, 1024, __VA_ARGS__);\
	mylog(c, _str_);\
}
#define LOGE(...) LOG('R', "ERR("MOD"): " __VA_ARGS__)
#define LOGW(...) LOG('G', "WRN("MOD"): " __VA_ARGS__)
#define LOGI(...) LOG('B', "INF("MOD"): " __VA_ARGS__)
#define LOGT(...) LOG('K', "TRC("MOD"): " __VA_ARGS__)

