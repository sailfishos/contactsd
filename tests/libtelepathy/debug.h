/*
 * Copyright © 2011 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2011 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TEST_DEBUG_H__
#define __TEST_DEBUG_H__

#include <glib.h>

#undef DEBUG
#define DEBUG(format, ...) \
  _test_log ("%s: " format, G_STRFUNC, ##__VA_ARGS__)

G_BEGIN_DECLS

void test_debug_enable (gboolean enable);

G_END_DECLS

#endif
