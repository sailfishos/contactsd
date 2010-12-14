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

#include <telepathy-glib/base-contact-list.h>

G_BEGIN_DECLS

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

typedef struct _TestContactListManager TestContactListManager;
typedef struct _TestContactListManagerClass TestContactListManagerClass;
typedef struct _TestContactListManagerPrivate TestContactListManagerPrivate;

struct _TestContactListManagerClass {
    TpBaseContactListClass parent_class;
};

struct _TestContactListManager {
    TpBaseContactList parent;

    TestContactListManagerPrivate *priv;
};

GType test_contact_list_manager_get_type (void);

void test_contact_list_manager_add_to_group (TestContactListManager *self,
    const gchar *group_name, TpHandle member);
void test_contact_list_manager_remove_from_group (TestContactListManager *self,
    const gchar *group_name, TpHandle member);

void test_contact_list_manager_request_subscription (TestContactListManager *self,
    TpHandle member,  const gchar *message);
void test_contact_list_manager_unsubscribe (TestContactListManager *self,
    TpHandle member);
void test_contact_list_manager_authorize_publication (TestContactListManager *self,
    TpHandle member);
void test_contact_list_manager_unpublish (TestContactListManager *self,
    TpHandle member);
void test_contact_list_manager_remove_contact (TestContactListManager *self,
    TpHandle member);

G_END_DECLS

#endif
