/*
 * contact-list-conn.h - header for an example connection
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2010 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TEST_CONTACT_LIST_CONN_H__
#define __TEST_CONTACT_LIST_CONN_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/contacts-mixin.h>
#include <telepathy-glib/presence-mixin.h>

#include "contact-list-manager.h"

G_BEGIN_DECLS

typedef struct _TestContactListConnection TestContactListConnection;
typedef struct _TestContactListConnectionClass TestContactListConnectionClass;
typedef struct _TestContactListConnectionPrivate TestContactListConnectionPrivate;

struct _TestContactListConnectionClass {
    TpBaseConnectionClass parent_class;

    TpPresenceMixinClass presence_mixin;
    TpContactsMixinClass contacts_mixin;
    TpDBusPropertiesMixinClass properties_class;
};

struct _TestContactListConnection {
    TpBaseConnection parent;
    TpPresenceMixin presence_mixin;
    TpContactsMixin contacts_mixin;

    TestContactListConnectionPrivate *priv;
};

GType test_contact_list_connection_get_type (void);

#define TEST_TYPE_CONTACT_LIST_CONNECTION \
  (test_contact_list_connection_get_type ())
#define TEST_CONTACT_LIST_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TEST_TYPE_CONTACT_LIST_CONNECTION, \
                              TestContactListConnection))
#define TEST_CONTACT_LIST_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_CONTACT_LIST_CONNECTION, \
                           TestContactListConnectionClass))
#define TEST_IS_CONTACT_LIST_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TEST_TYPE_CONTACT_LIST_CONNECTION))
#define TEST_IS_CONTACT_LIST_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_CONTACT_LIST_CONNECTION))
#define TEST_CONTACT_LIST_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_CONTACT_LIST_CONNECTION, \
                              TestContactListConnectionClass))

gchar *test_contact_list_normalize_contact (TpHandleRepoIface *repo,
    const gchar *id, gpointer context, GError **error);

TestContactListManager *test_contact_list_connection_get_contact_list_manager (
    TestContactListConnection *self);

G_END_DECLS

#endif
