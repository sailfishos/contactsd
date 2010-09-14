/*
 * Example channel manager for contact lists
 *
 * Copyright © 2007-2010 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2010 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TEST_CONTACT_LIST_MANAGER_H__
#define __TEST_CONTACT_LIST_MANAGER_H__

#include <glib-object.h>

#include <telepathy-glib/channel-manager.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/presence-mixin.h>

G_BEGIN_DECLS

typedef struct _TestContactListManager TestContactListManager;
typedef struct _TestContactListManagerClass TestContactListManagerClass;
typedef struct _TestContactListManagerPrivate TestContactListManagerPrivate;

struct _TestContactListManagerClass {
    GObjectClass parent_class;
};

struct _TestContactListManager {
    GObject parent;

    TestContactListManagerPrivate *priv;
};

GType test_contact_list_manager_get_type (void);

typedef struct _TestContactListAvatarData TestContactListAvatarData;

struct _TestContactListAvatarData
{
    GArray *data;
    gchar *mime_type;
    gchar *token;
};

#define TEST_TYPE_CONTACT_LIST_MANAGER \
  (test_contact_list_manager_get_type ())
#define TEST_CONTACT_LIST_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TEST_TYPE_CONTACT_LIST_MANAGER, \
                              TestContactListManager))
#define TEST_CONTACT_LIST_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_CONTACT_LIST_MANAGER, \
                           TestContactListManagerClass))
#define TEST_IS_CONTACT_LIST_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TEST_TYPE_CONTACT_LIST_MANAGER))
#define TEST_IS_CONTACT_LIST_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_CONTACT_LIST_MANAGER))
#define TEST_CONTACT_LIST_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_CONTACT_LIST_MANAGER, \
                              TestContactListManagerClass))

gboolean test_contact_list_manager_add_to_group (
    TestContactListManager *self, GObject *channel,
    TpHandle group, TpHandle member, const gchar *message, GError **error);

gboolean test_contact_list_manager_remove_from_group (
    TestContactListManager *self, GObject *channel,
    TpHandle group, TpHandle member, const gchar *message, GError **error);

/* elements 1, 2... of this enum must be kept in sync with elements 0, 1...
 * of the array _contact_lists in contact-list-manager.h */
typedef enum {
    INVALID_TEST_CONTACT_LIST,
    TEST_CONTACT_LIST_SUBSCRIBE = 1,
    TEST_CONTACT_LIST_PUBLISH,
    TEST_CONTACT_LIST_STORED,
    NUM_TEST_CONTACT_LISTS
} TestContactListHandle;

/* this enum must be kept in sync with the array _statuses in
 * contact-list-manager.c */
typedef enum {
    TEST_CONTACT_LIST_PRESENCE_OFFLINE = 0,
    TEST_CONTACT_LIST_PRESENCE_UNKNOWN,
    TEST_CONTACT_LIST_PRESENCE_ERROR,
    TEST_CONTACT_LIST_PRESENCE_AWAY,
    TEST_CONTACT_LIST_PRESENCE_AVAILABLE
} TestContactListPresence;

const TpPresenceStatusSpec *test_contact_list_presence_statuses (
    void);

gboolean test_contact_list_manager_add_to_list (
    TestContactListManager *self, GObject *channel,
    TestContactListHandle list, TpHandle member, const gchar *message,
    GError **error);

gboolean test_contact_list_manager_remove_from_list (
    TestContactListManager *self, GObject *channel,
    TestContactListHandle list, TpHandle member, const gchar *message,
    GError **error);

const gchar **test_contact_lists (void);

TestContactListPresence test_contact_list_manager_get_presence (
    TestContactListManager *self, TpHandle contact);

const gchar *test_contact_list_manager_get_alias (
    TestContactListManager *self, TpHandle contact);
void test_contact_list_manager_set_alias (
    TestContactListManager *self, TpHandle contact, const gchar *alias);

const TestContactListAvatarData *test_contact_list_manager_get_avatar (
    TestContactListManager *self, TpHandle contact);
void test_contact_list_manager_set_avatar (
    TestContactListManager *self, TpHandle contact,
    const TestContactListAvatarData *avatar_data);

static TestContactListAvatarData *test_contact_list_avatar_data_new (
    GArray *data, const gchar *mime_type, const gchar *token);
static void test_contact_list_avatar_data_free (gpointer data);

G_END_DECLS

#endif
