/*
 * simple-account.c - a simple account service.
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "simple-account.h"

#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/svc-account.h>

static void account_iface_init (gpointer, gpointer);

G_DEFINE_TYPE_WITH_CODE (TpTestsSimpleAccount,
    tp_tests_simple_account,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_ACCOUNT,
        account_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init)
    )

/* TP_IFACE_ACCOUNT is implied */
static const char *ACCOUNT_INTERFACES[] = { NULL };

enum
{
  PROP_0,
  PROP_INTERFACES,
  PROP_DISPLAY_NAME,
  PROP_ICON,
  PROP_VALID,
  PROP_ENABLED,
  PROP_NICKNAME,
  PROP_PARAMETERS,
  PROP_AUTOMATIC_PRESENCE,
  PROP_CONNECT_AUTO,
  PROP_CONNECTION,
  PROP_CONNECTION_STATUS,
  PROP_CONNECTION_STATUS_REASON,
  PROP_CURRENT_PRESENCE,
  PROP_REQUESTED_PRESENCE,
  PROP_NORMALIZED_NAME,
  PROP_HAS_BEEN_ONLINE,
};

struct _TpTestsSimpleAccountPrivate
{
  TpConnection *connection;
  TpConnectionStatus connection_status;
  TpConnectionStatusReason connection_status_reason;
  gboolean has_been_online;
  gboolean enabled;

  GHashTable *parameters;

  TpContact *self_contact;
  gchar *nickname;
  gchar *normalized_name;
  GValueArray *current_presence;
};

static void connection_invalidated_cb (TpProxy *proxy, guint domain, gint code,
    gchar *message, TpTestsSimpleAccount *self);

static void
account_iface_init (gpointer klass,
    gpointer unused G_GNUC_UNUSED)
{
#define IMPLEMENT(x) tp_svc_account_implement_##x (\
  klass, tp_tests_simple_account_##x)
  /* TODO */
#undef IMPLEMENT
}


static void
tp_tests_simple_account_init (TpTestsSimpleAccount *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      TpTestsSimpleAccountPrivate);

  self->priv->connection_status = TP_CONNECTION_STATUS_DISCONNECTED;
  self->priv->connection_status_reason = TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
  self->priv->has_been_online = FALSE;
  self->priv->enabled = TRUE;

  self->priv->parameters = g_hash_table_new (NULL, NULL);

  self->priv->nickname = g_strdup ("");
  self->priv->normalized_name = g_strdup ("");
  self->priv->current_presence = tp_value_array_build (3,
      G_TYPE_UINT, TP_CONNECTION_PRESENCE_TYPE_OFFLINE,
      G_TYPE_STRING, "offline",
      G_TYPE_STRING, "",
      G_TYPE_INVALID);
}

static void
tp_tests_simple_account_get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  TpTestsSimpleAccount *self = TP_TESTS_SIMPLE_ACCOUNT (object);
  GValueArray *offline_presence;

  offline_presence = tp_value_array_build (3,
      G_TYPE_UINT, TP_CONNECTION_PRESENCE_TYPE_OFFLINE,
      G_TYPE_STRING, "offline",
      G_TYPE_STRING, "",
      G_TYPE_INVALID);

  switch (property_id) {
    case PROP_INTERFACES:
      g_value_set_boxed (value, ACCOUNT_INTERFACES);
      break;
    case PROP_DISPLAY_NAME:
      g_value_set_string (value, "Fake Account");
      break;
    case PROP_ICON:
      g_value_set_string (value, "");
      break;
    case PROP_VALID:
      g_value_set_boolean (value, TRUE);
      break;
    case PROP_ENABLED:
      g_value_set_boolean (value, self->priv->enabled);
      break;
    case PROP_NICKNAME:
      g_value_set_string (value, self->priv->nickname);
      break;
    case PROP_PARAMETERS:
      g_value_set_boxed (value, self->priv->parameters);
      break;
    case PROP_AUTOMATIC_PRESENCE:
      g_value_set_boxed (value, offline_presence);
      break;
    case PROP_CONNECT_AUTO:
      g_value_set_boolean (value, FALSE);
      break;
    case PROP_CONNECTION:
      if (self->priv->connection != NULL)
        g_value_set_boxed (value, tp_proxy_get_object_path (self->priv->connection));
      else
        g_value_set_boxed (value, "/");
      break;
    case PROP_CONNECTION_STATUS:
      g_value_set_uint (value, self->priv->connection_status);
      break;
    case PROP_CONNECTION_STATUS_REASON:
      g_value_set_uint (value, self->priv->connection_status_reason);
      break;
    case PROP_CURRENT_PRESENCE:
      g_value_set_boxed (value, self->priv->current_presence);
      break;
    case PROP_REQUESTED_PRESENCE:
      g_value_set_boxed (value, offline_presence);
      break;
    case PROP_NORMALIZED_NAME:
      g_value_set_string (value, self->priv->normalized_name);
      break;
    case PROP_HAS_BEEN_ONLINE:
      g_value_set_boolean (value, self->priv->has_been_online);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
  }

  g_boxed_free (TP_STRUCT_TYPE_SIMPLE_PRESENCE, offline_presence);
}

static void
tp_tests_simple_account_set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *spec)
{
  TpTestsSimpleAccount *self = TP_TESTS_SIMPLE_ACCOUNT (object);

  switch (property_id) {
    case PROP_PARAMETERS:
      if (g_value_get_boxed (value) != NULL)
        {
          if (self->priv->parameters)
            g_hash_table_unref (self->priv->parameters);
          self->priv->parameters = g_value_dup_boxed (value);
        }
      else
        {
          g_hash_table_remove_all (self->priv->parameters);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
      break;
  }
}

static void
tp_tests_simple_account_finalize (GObject *object)
{
  TpTestsSimpleAccount *self = TP_TESTS_SIMPLE_ACCOUNT (object);

  if (self->priv->connection)
    {
      g_signal_handlers_disconnect_by_func (self->priv->connection,
          connection_invalidated_cb, self);
      g_object_unref (self->priv->connection);
    }

  if (self->priv->parameters)
    g_hash_table_unref (self->priv->parameters);

  if (self->priv->self_contact != NULL)
    g_object_unref (self->priv->self_contact);

  g_free (self->priv->nickname);
  g_free (self->priv->normalized_name);
  g_value_array_free (self->priv->current_presence);

  G_OBJECT_CLASS (tp_tests_simple_account_parent_class)->finalize(object);
}

/**
  * This class currently only provides the minimum for
  * tp_account_prepare to succeed. This turns out to be only a working
  * Properties.GetAll().
  */
static void
tp_tests_simple_account_class_init (TpTestsSimpleAccountClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  static TpDBusPropertiesMixinPropImpl a_props[] = {
        { "Interfaces", "interfaces", NULL },
        { "DisplayName", "display-name", NULL },
        { "Icon", "icon", NULL },
        { "Valid", "valid", NULL },
        { "Enabled", "enabled", NULL },
        { "Nickname", "nickname", NULL },
        { "Parameters", "parameters", NULL },
        { "AutomaticPresence", "automatic-presence", NULL },
        { "ConnectAutomatically", "connect-automatically", NULL },
        { "Connection", "connection", NULL },
        { "ConnectionStatus", "connection-status", NULL },
        { "ConnectionStatusReason", "connection-status-reason", NULL },
        { "CurrentPresence", "current-presence", NULL },
        { "RequestedPresence", "requested-presence", NULL },
        { "NormalizedName", "normalized-name", NULL },
        { "HasBeenOnline", "has-been-online", NULL },
        { NULL }
  };

  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
        { TP_IFACE_ACCOUNT,
          tp_dbus_properties_mixin_getter_gobject_properties,
          NULL,
          a_props
        },
        { NULL },
  };

  g_type_class_add_private (klass, sizeof (TpTestsSimpleAccountPrivate));
  object_class->set_property = tp_tests_simple_account_set_property;
  object_class->get_property = tp_tests_simple_account_get_property;
  object_class->finalize = tp_tests_simple_account_finalize;

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "In this case we only implement Account, so none.",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_string ("display-name", "display name",
      "DisplayName property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPLAY_NAME, param_spec);

  param_spec = g_param_spec_string ("icon", "icon",
      "Icon property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ICON, param_spec);

  param_spec = g_param_spec_boolean ("valid", "valid",
      "Valid property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VALID, param_spec);

  param_spec = g_param_spec_boolean ("enabled", "enabled",
      "Enabled property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ENABLED, param_spec);

  param_spec = g_param_spec_string ("nickname", "nickname",
      "Nickname property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NICKNAME, param_spec);

  param_spec = g_param_spec_boxed ("parameters", "parameters",
      "Parameters property",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PARAMETERS, param_spec);

  param_spec = g_param_spec_boxed ("automatic-presence", "automatic presence",
      "AutomaticPresence property",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_AUTOMATIC_PRESENCE,
      param_spec);

  param_spec = g_param_spec_boolean ("connect-automatically",
      "connect automatically", "ConnectAutomatically property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECT_AUTO, param_spec);

  param_spec = g_param_spec_boxed ("connection", "connection",
      "Connection property",
      DBUS_TYPE_G_OBJECT_PATH,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_uint ("connection-status", "connection status",
      "ConnectionStatus property",
      0, NUM_TP_CONNECTION_STATUSES, TP_CONNECTION_STATUS_DISCONNECTED,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS,
      param_spec);

  param_spec = g_param_spec_uint ("connection-status-reason",
      "connection status reason", "ConnectionStatusReason property",
      0, NUM_TP_CONNECTION_STATUS_REASONS,
      TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS_REASON,
      param_spec);

  param_spec = g_param_spec_boxed ("current-presence", "current presence",
      "CurrentPresence property",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CURRENT_PRESENCE,
      param_spec);

  param_spec = g_param_spec_boxed ("requested-presence", "requested presence",
      "RequestedPresence property",
      TP_STRUCT_TYPE_SIMPLE_PRESENCE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED_PRESENCE,
      param_spec);

  param_spec = g_param_spec_string ("normalized-name", "normalized name",
      "NormalizedName property",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_NORMALIZED_NAME,
      param_spec);

  param_spec = g_param_spec_boolean ("has-been-online", "has been online",
      "HasBeenOnline property",
      FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_HAS_BEEN_ONLINE,
      param_spec);

  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpTestsSimpleAccountClass, dbus_props_class));
}

static void
remove_connection (TpTestsSimpleAccount *self)
{
  GHashTable *change;

  self->priv->connection_status = tp_connection_get_status (self->priv->connection,
      &self->priv->connection_status_reason);

  g_signal_handlers_disconnect_by_func (self->priv->connection,
      connection_invalidated_cb, self);
  g_object_unref (self->priv->connection);
  self->priv->connection = NULL;

  if (self->priv->self_contact != NULL)
    {
      g_object_unref (self->priv->self_contact);
      self->priv->self_contact = NULL;
    }

  g_value_array_free (self->priv->current_presence);
  self->priv->current_presence = tp_value_array_build (3,
      G_TYPE_UINT, TP_CONNECTION_PRESENCE_TYPE_OFFLINE,
      G_TYPE_STRING, "offline",
      G_TYPE_STRING, "",
      G_TYPE_INVALID);

  change = tp_asv_new (NULL, NULL);
  tp_asv_set_object_path (change, "Connection", "/");
  tp_asv_set_uint32 (change, "ConnectionStatus", self->priv->connection_status);
  tp_asv_set_uint32 (change, "ConnectionStatusReason", self->priv->connection_status_reason);
  tp_asv_set_boxed (change, "CurrentPresence", TP_STRUCT_TYPE_SIMPLE_PRESENCE, self->priv->current_presence);
  tp_svc_account_emit_account_property_changed (self, change);
  g_hash_table_unref (change);
}

static void
connection_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    TpTestsSimpleAccount *self)
{
  remove_connection (self);
}

static void
on_alias_notify_cb (TpContact *self_contact,
    GParamSpec *pspec,
    TpTestsSimpleAccount *self)
{
  GHashTable *change;

  g_free (self->priv->nickname);
  self->priv->nickname = g_strdup (tp_contact_get_alias (self->priv->self_contact));

  change = tp_asv_new (NULL, NULL);
  tp_asv_set_string (change, "Nickname", self->priv->nickname);
  tp_svc_account_emit_account_property_changed (self, change);
  g_hash_table_unref (change);
}

static void
on_presence_changed_cb (TpContact *self_contact,
    guint type,
    gchar *status,
    gchar *message,
    TpTestsSimpleAccount *self)
{
  GHashTable *change;

  g_value_array_free (self->priv->current_presence);
  self->priv->current_presence = tp_value_array_build (3,
      G_TYPE_UINT, tp_contact_get_presence_type (self->priv->self_contact),
      G_TYPE_STRING, tp_contact_get_presence_status (self->priv->self_contact),
      G_TYPE_STRING, tp_contact_get_presence_message (self->priv->self_contact),
      G_TYPE_INVALID);

  change = tp_asv_new (NULL, NULL);
  tp_asv_set_boxed (change, "CurrentPresence", TP_STRUCT_TYPE_SIMPLE_PRESENCE, self->priv->current_presence);
  tp_svc_account_emit_account_property_changed (self, change);
  g_hash_table_unref (change);
}

static void
got_self_contact_cb (TpConnection *connection,
    guint n_contacts,
    TpContact * const *contacts,
    guint n_failed,
    const TpHandle *failed,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  TpTestsSimpleAccount *self = user_data;
  GHashTable *change;

  if (n_failed != 0)
    return;

  self->priv->self_contact = g_object_ref (contacts[0]);
  self->priv->connection_status = tp_connection_get_status (self->priv->connection,
      &self->priv->connection_status_reason);
  self->priv->has_been_online = TRUE;

  g_free (self->priv->normalized_name);
  self->priv->normalized_name = g_strdup (tp_contact_get_identifier (self->priv->self_contact));

  g_free (self->priv->nickname);
  self->priv->nickname = g_strdup (tp_contact_get_alias (self->priv->self_contact));
  g_signal_connect (self->priv->self_contact, "notify::alias",
      G_CALLBACK (on_alias_notify_cb), self);

  g_value_array_free (self->priv->current_presence);
  self->priv->current_presence = tp_value_array_build (3,
      G_TYPE_UINT, tp_contact_get_presence_type (self->priv->self_contact),
      G_TYPE_STRING, tp_contact_get_presence_status (self->priv->self_contact),
      G_TYPE_STRING, tp_contact_get_presence_message (self->priv->self_contact),
      G_TYPE_INVALID);
  g_signal_connect (self->priv->self_contact, "presence-changed",
      G_CALLBACK (on_presence_changed_cb), self);

  change = tp_asv_new (NULL, NULL);
  tp_asv_set_object_path (change, "Connection", tp_proxy_get_object_path (self->priv->connection));
  tp_asv_set_uint32 (change, "ConnectionStatus", self->priv->connection_status);
  tp_asv_set_uint32 (change, "ConnectionStatusReason", self->priv->connection_status_reason);
  tp_asv_set_boolean (change, "HasBeenOnline", self->priv->has_been_online);
  tp_asv_set_boxed (change, "CurrentPresence", TP_STRUCT_TYPE_SIMPLE_PRESENCE, self->priv->current_presence);
  tp_asv_set_string (change, "Nickname", self->priv->nickname);
  tp_asv_set_string (change, "NormalizedName", self->priv->normalized_name);
  tp_svc_account_emit_account_property_changed (self, change);
  g_hash_table_unref (change);
}

static void
connection_ready_cb (TpConnection *connection,
    const GError *error,
    gpointer user_data)
{
  TpTestsSimpleAccount *self = user_data;
  TpHandle self_handle;
  TpContactFeature features[] = { TP_CONTACT_FEATURE_ALIAS, TP_CONTACT_FEATURE_PRESENCE };

  if (error != NULL)
    return;

  self_handle = tp_connection_get_self_handle (self->priv->connection);
  tp_connection_get_contacts_by_handle (self->priv->connection,
      1, &self_handle, G_N_ELEMENTS (features), features,
      got_self_contact_cb, self, NULL, G_OBJECT (self));
}

void
tp_tests_simple_account_set_connection (TpTestsSimpleAccount *self,
    const gchar *object_path)
{
  if (self->priv->connection != NULL)
    remove_connection (self);

  if (object_path != NULL && tp_strdiff (object_path, "/"))
    {
      TpDBusDaemon *dbus = tp_dbus_daemon_dup (NULL);
      self->priv->connection = tp_connection_new (dbus, NULL, object_path, NULL);
      g_signal_connect (self->priv->connection, "invalidated",
          G_CALLBACK (connection_invalidated_cb), self);
      tp_connection_call_when_ready (self->priv->connection,
          connection_ready_cb, self);
      g_object_unref (dbus);
    }
}

void
tp_tests_simple_account_removed (TpTestsSimpleAccount *self)
{
  tp_svc_account_emit_removed (self);
}

void
tp_tests_simple_account_set_enabled (TpTestsSimpleAccount *self, gboolean enabled)
{
  GHashTable *change;
  self->priv->enabled = enabled;
  change = tp_asv_new (NULL, NULL);
  tp_asv_set_boolean (change, "Enabled", enabled);
  tp_svc_account_emit_account_property_changed(self, change);
  g_hash_table_unref (change);
}
