/** This file is part of Contacts daemon
 **
 ** Copyright (c) 2010-2011 Nokia Corporation and/or its subsidiary(-ies).
 **
 ** Contact:  Nokia Corporation (info@qt.nokia.com)
 **
 ** GNU Lesser General Public License Usage
 ** This file may be used under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation and appearing in the
 ** file LICENSE.LGPL included in the packaging of this file.  Please review the
 ** following information to ensure the GNU Lesser General Public License version
 ** 2.1 requirements will be met:
 ** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Nokia gives you certain additional rights.
 ** These rights are described in the Nokia Qt LGPL Exception version 1.1, included
 ** in the file LGPL_EXCEPTION.txt in this package.
 **
 ** Other Usage
 ** Alternatively, this file may be used in accordance with the terms and
 ** conditions contained in a signed written agreement between you and Nokia.
 **/

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

/*!
 * \enum Contactsd::ErrorCode
 * Error codes
 *
 * \value ErrorNoSpace - No disk space left for import operation
 * \value ErrorUnknown - Unknown error occured
 */
enum ErrorCode {
    ErrorUnknown = 1,
    ErrorNoSpace,
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
