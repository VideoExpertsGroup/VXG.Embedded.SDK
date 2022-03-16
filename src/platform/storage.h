#ifndef __OBJECT_STORAGE_H
#define __OBJECT_STORAGE_H

#include <agent-proto/objects/config.h>
#include <string>
#include <vector>

//! Objects storage base class
/*!
  Class provides structure of the object storage, all it's virtual methids
  should be implemented in the derived class.
*/
class object_storage {
    std::string path_;

public:
    object_storage() {};
    virtual ~object_storage() {};

    //! Stored object type
    enum media_type {
        OT_MP4,  /*!< Video clip in MP4 container. */
        OT_JPEG, /*!< Video snapshot image in JPEG format. */

        OT_INVALID = -1 /*!< Invalid value */
    };

    //! Trigger caused the object.
    enum trigger_type {
        TT_ANY,    /*!< Any trigger. */
        TT_MOTION, /*!< Motion trigger. */

        TT_INVALID = -1 /*!< Invalid value. */
    };

    //! Free space in storage.
    /*!
        \return Free size in bytes.
    */
    virtual size_t free() = 0;
    //! Used space in storage.
    /*!
        \return Used size in bytes.
    */
    virtual size_t used() = 0;
    //! Size of the storage.
    /*!
        \return Size of storage in bytes.
    */
    virtual size_t size() = 0;

    //! Internal mount point of the storage.
    /*!
        \return Path.
    */
    virtual std::string path() = 0;

    //! Search for objects in storage.
    /*!
        /return List of the unique objects identifiers.
                Identifiers could be for ex. paths or internal ids.
    */
    virtual std::vector<vxg::cloud::agent::proto::video_clip_info> search(
        trigger_type ttype,
        media_type otype,
        std::time_t time_start,
        std::time_t time_stop) = 0;
};

#endif  // __OBJECT_STORAGE_H