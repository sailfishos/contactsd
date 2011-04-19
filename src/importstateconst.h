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

#ifndef IMPORTSTATECONST_H_
#define IMPORTSTATECONST_H_

#include <QLatin1String>

namespace Contactsd {

/*!
 * \enum Contactsd::AccountImportState
 * Each account's contacts importing state.
 *
 * \value Importing - account is actively importing contacts
 * \value Imported - account has finished importing contacts,
 *                   but UI has not yet handled these contacts
 * \value Finished - account has finished importing contacts,
 *                   and UI has handled these contacts (e.g. merged them)
 */
enum AccountImportState {
    Importing = 1,
    Imported,
    Finished
};

const QLatin1String SettingsOrganization("Nokia");
const QLatin1String SettingsApplication("Contactsd");

/*!
 * This is used as the "account path" for MfE or Sync framework to update the import progress.
 * When we get non-service contacts from MfE or Sync, we can check the import state by using this string.
 */
const QLatin1String MfeSync("MfeSync");
}

#endif // IMPORTSTATECONST_H_
