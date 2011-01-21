/*
 * Copyright © 2011 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2011 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "debug.h"

static gboolean enabled = FALSE;

void
_test_log (const gchar *format, ...)
{
  if (enabled)
    {
      va_list args;
      va_start (args, format);
      g_logv ("test", G_LOG_LEVEL_DEBUG, format, args);
      va_end (args);
    }
}

void
test_debug_enable (gboolean enable)
{
  enabled = enable;
}

