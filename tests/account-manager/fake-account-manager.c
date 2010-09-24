/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (people-users@projects.maemo.org)
**
** This file is part of contactsd.
**
** If you have questions regarding the use of this file, please contact
** Nokia at people-users@projects.maemo.org.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include <telepathy-glib/telepathy-glib.h>

#include "tests/lib/glib/simple-account-manager.h"
#include "tests/lib/glib/simple-account.h"
#include "tests/lib/glib/util.h"

#define BUS_NAME "org.maemo.Contactsd.UnitTest"
#define ACCOUNT_PATH TP_ACCOUNT_OBJECT_PATH_BASE "fakecm/fakeproto/fakeaccount"
#define CONNECTION_PATH TP_CONN_OBJECT_PATH_BASE "fakecm/fakeproto/fakeaccount"

typedef struct {
  GMainLoop *loop;
  TpTestsSimpleAccount *account;
  gboolean started;
} Context;

static void
watch_name_owner_cb (TpDBusDaemon *bus_daemon,
    const gchar *name,
    const gchar *new_owner,
    gpointer user_data)
{
  Context *ctx = user_data;

  if (tp_strdiff (name, BUS_NAME))
    return;

  if (!tp_str_empty (new_owner))
    {
      /* The UnitTest is now started, it should have registered that Connection,
       * so we can now set the Account online with that connection. */
      ctx->started = TRUE;
      tp_tests_simple_account_set_connection (ctx->account, CONNECTION_PATH,
          TP_CONNECTION_STATUS_CONNECTED,
          TP_CONNECTION_STATUS_REASON_REQUESTED);
    }
  else if (ctx->started)
    {
      g_main_loop_quit (ctx->loop);
    }
}

int
main(int argc, char **argv)
{
  Context ctx = { 0, };
  TpDBusDaemon *dbus;
  TpTestsSimpleAccountManager *manager;
  GHashTable *asv;

  g_type_init ();

  dbus = tp_dbus_daemon_dup (NULL);

  /* Create a fake AccountManager */
  manager = tp_tests_object_new_static_class (
      TP_TESTS_TYPE_SIMPLE_ACCOUNT_MANAGER, NULL);
  tp_dbus_daemon_register_object (dbus, TP_ACCOUNT_MANAGER_OBJECT_PATH,
      manager);
  tp_dbus_daemon_request_name (dbus, TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, NULL);

  /* Create a fake Account */
  asv = tp_asv_new ("account", G_TYPE_STRING, "fake@account.org", NULL);
  ctx.account = tp_tests_object_new_static_class (TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      "parameters", asv,
      NULL);
  tp_dbus_daemon_register_object (dbus, ACCOUNT_PATH, ctx.account);

  /* Set the Account on the AccountManager */
  tp_tests_simple_account_manager_add_account (manager, ACCOUNT_PATH, TRUE);

  /* Wait for the UnitTest to appear on the bus */
  tp_dbus_daemon_watch_name_owner (dbus, BUS_NAME, watch_name_owner_cb,
      &ctx, NULL);

  /* Let's go */
  ctx.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (ctx.loop);

  /* Cleanup */
  g_object_unref (dbus);
  g_object_unref (manager);
  g_hash_table_unref (asv);
  g_object_unref (ctx.account);
  g_main_loop_unref (ctx.loop);

  return 0;
}
