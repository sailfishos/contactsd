/*
 * Example ContactList channels with handle type LIST or GROUP
 *
 * Copyright © 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef TEST_CONTACT_LIST_H
#define TEST_CONTACT_LIST_H

#include <glib-object.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/group-mixin.h>

G_BEGIN_DECLS

typedef struct _TestContactListBase TestContactListBase;
typedef struct _TestContactListBaseClass TestContactListBaseClass;
typedef struct _TestContactListBasePrivate TestContactListBasePrivate;

typedef struct _TestContactList TestContactList;
typedef struct _TestContactListClass TestContactListClass;
typedef struct _TestContactListPrivate TestContactListPrivate;

typedef struct _ExampleContactGroup ExampleContactGroup;
typedef struct _ExampleContactGroupClass ExampleContactGroupClass;
typedef struct _ExampleContactGroupPrivate ExampleContactGroupPrivate;

GType test_contact_list_base_get_type (void);
GType test_contact_list_get_type (void);
GType test_contact_group_get_type (void);

#define TEST_TYPE_CONTACT_LIST_BASE \
  (test_contact_list_base_get_type ())
#define TEST_CONTACT_LIST_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_CONTACT_LIST_BASE, \
                               TestContactListBase))
#define TEST_CONTACT_LIST_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_CONTACT_LIST_BASE, \
                            TestContactListBaseClass))
#define TEST_IS_CONTACT_LIST_BASE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_CONTACT_LIST_BASE))
#define TEST_IS_CONTACT_LIST_BASE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_CONTACT_LIST_BASE))
#define TEST_CONTACT_LIST_BASE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_CONTACT_LIST_BASE, \
                              TestContactListBaseClass))

#define TEST_TYPE_CONTACT_LIST \
  (test_contact_list_get_type ())
#define TEST_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_CONTACT_LIST, \
                               TestContactList))
#define TEST_CONTACT_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_CONTACT_LIST, \
                            TestContactListClass))
#define TEST_IS_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_CONTACT_LIST))
#define TEST_IS_CONTACT_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_CONTACT_LIST))
#define TEST_CONTACT_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_CONTACT_LIST, \
                              TestContactListClass))

#define TEST_TYPE_CONTACT_GROUP \
  (test_contact_group_get_type ())
#define TEST_CONTACT_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_CONTACT_GROUP, \
                               ExampleContactGroup))
#define TEST_CONTACT_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_CONTACT_GROUP, \
                            ExampleContactGroupClass))
#define TEST_IS_CONTACT_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_CONTACT_GROUP))
#define TEST_IS_CONTACT_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_CONTACT_GROUP))
#define TEST_CONTACT_GROUP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_CONTACT_GROUP, \
                              ExampleContactGroupClass))

struct _TestContactListBaseClass {
    GObjectClass parent_class;
    TpGroupMixinClass group_class;
    TpDBusPropertiesMixinClass dbus_properties_class;
};

struct _TestContactListClass {
    TestContactListBaseClass parent_class;
};

struct _ExampleContactGroupClass {
    TestContactListBaseClass parent_class;
};

struct _TestContactListBase {
    GObject parent;
    TpGroupMixin group;
    TestContactListBasePrivate *priv;
};

struct _TestContactList {
    TestContactListBase parent;
    TestContactListPrivate *priv;
};

struct _ExampleContactGroup {
    TestContactListBase parent;
    ExampleContactGroupPrivate *priv;
};

G_END_DECLS

#endif
