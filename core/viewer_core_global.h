#pragma once

#include <QtCore/qglobal.h>

#if defined(VIEWER_CORE_LIBRARY)
#  define VIEWER_CORE_EXPORT Q_DECL_EXPORT
#else
#  define VIEWER_CORE_EXPORT Q_DECL_IMPORT
#endif
