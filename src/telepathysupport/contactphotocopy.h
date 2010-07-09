#ifndef CONTACTPHOTOCOPY_H
#define CONTACTPHOTOCOPY_H

#include <QStringList>
#include <QSize>

/**
 * ContactPhotoCopy handles handling contact's local photo copying to
 * a correct avatar directory.
 */
class ContactPhotoCopy
{
public:
    /**
     * Returns the primary directory where the avatars are stored.
     * The directory may not exist, but it is automatically created
     * \return Directory where the avatars are stored.
     */
    static QString avatarDir();

    /**
     * \brief Copies the given file to the avatar directory.
     * If the avatarDir() doesn't exist, it will be created.
     * \param[in] fullFilePath The full path to the file to be copied.
     * \param[out] newFileName The new name of the file. This can be different than the original filename, if the given file already exists.
     * \return True if the copy succeeds.
     * \sa avatarDir()
     */
    static bool copyToAvatarDir(const QString &fullFilePath, QString &newFileName);
    /**
     * \bried Copies the given file to the avatar directory scaled and cropped.
     * If the avatarDir() doesn't exist, it will be created.
     * param[in] fullFilePath The full path to the file to be copied
     * param[in] scaledSize Size to which the original image should be scaled and cropped. Currently the given size,
     * should be square. Width == height.
     * param[out] newFileName The new name of the file. This can be different than the original filename, if the given file already exists.
     * \return True if the copy succeeds.
     * \sa avatarDir()
     */
    static bool copyToAvatarDir(const QString& fullFilePath, const QSize& scaledSize, QString& newFileName);

    /**
     * \brief Removes the given file from the primary avatar dir
     * \sa avatarDir()
     */
    static void removeFromAvatarDir(const QString &fileName);

    /**
     * \brief Returns a list of search paths.
     * \return List of search paths
     */
    static QStringList searchPaths();
private:
        enum PhotoEditType
        {
            PhotoEditLeaveUntouched = 0,
            PhotoEditCropAndScale = 1
        };
private:
    static void initializeSearchPaths();
    static bool copyToAvatarDir(const QString& fullFilePath, const QSize& scaledSize,
                                PhotoEditType photoEditType, QString& newFileName);
    static QStringList avatarSearchPaths;
};

#endif //CONTACTPHOTOCOPY_H
