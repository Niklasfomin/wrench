/**
 * Copyright (c) 2017-2020. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_SIMPLESTORAGESERVICE_H
#define WRENCH_SIMPLESTORAGESERVICE_H

#include "wrench/services/storage/storage_helpers/FileTransferThread.h"
#include "wrench/services/storage/StorageService.h"
#include "SimpleStorageServiceProperty.h"
#include "SimpleStorageServiceMessagePayload.h"
#include "wrench/services/memory/MemoryManager.h"
#include "wrench/simgrid_S4U_util/S4U_PendingCommunication.h"
#include "wrench/simgrid_S4U_util/S4U_CommPort.h"
#include <fsmod.hpp>

namespace wrench {

    class SimulationMessage;

    class SimulationTimestampFileCopyStart;

    class S4U_PendingCommunication;

    /**
     * @brief A storage service that provides direct access to some storage resources (e.g., one or more disks).
     *        An important (configurable) property of the storage service is
     *        SimpleStorageServiceProperty::BUFFER_SIZE (see documentation thereof), which defines the
     *        buffer size that the storage service uses. Specifically, when the storage service
     *        receives/sends data from/to the network, it does so in a loop over data "chunks",
     *        with pipelined network and disk I/O operations. The smaller the buffer size the more "fluid"
     *        the model, but the more time-consuming the simulation. A large buffer size, however, may
     *        lead to less realistic simulations. At the extreme, an infinite buffer size would correspond
     *        to fully sequential executions (first a network receive/send, and then a disk write/read).
     *        Setting the buffer size to "0" corresponds to a fully fluid model in which individual
     *        data chunk operations are not simulated, thus achieving both accuracy (unless one specifically wishes
     *        to study the effects of buffering) and quick simulation times. For now, setting the buffer
     *        size to "0" is not implemented. The default buffer size is 10 MiB (note that the user can
     *        always declare a disk with arbitrary bandwidth in the platform description XML).
     */
    class SimpleStorageService : public StorageService {

    protected:
        /** @brief Default property values */
        WRENCH_PROPERTY_COLLECTION_TYPE default_property_values = {
                {SimpleStorageServiceProperty::MAX_NUM_CONCURRENT_DATA_CONNECTIONS, "infinity"},
                {SimpleStorageServiceProperty::BUFFER_SIZE, "10000000"},// 10 MEGA BYTE
                {SimpleStorageServiceProperty::CACHING_BEHAVIOR, "NONE"}};

        /** @brief Default message payload values */
        WRENCH_MESSAGE_PAYLOADCOLLECTION_TYPE default_messagepayload_values = {
                {SimpleStorageServiceMessagePayload::STOP_DAEMON_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::DAEMON_STOPPED_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FREE_SPACE_REQUEST_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FREE_SPACE_ANSWER_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_DELETE_REQUEST_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_DELETE_ANSWER_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_LOOKUP_REQUEST_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_LOOKUP_ANSWER_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_COPY_REQUEST_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_COPY_ANSWER_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_WRITE_REQUEST_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_WRITE_ANSWER_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_READ_REQUEST_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
                {SimpleStorageServiceMessagePayload::FILE_READ_ANSWER_MESSAGE_PAYLOAD, S4U_CommPort::default_control_message_size},
        };


    public:
        using StorageService::createFile;
        using StorageService::deleteFile;
        using StorageService::hasFile;
        using StorageService::lookupFile;
        using StorageService::readFile;
        using StorageService::writeFile;

        ~SimpleStorageService() override;

        static SimpleStorageService *createSimpleStorageService(const std::string &hostname,
                                                                std::set<std::string> mount_points,
                                                                WRENCH_PROPERTY_COLLECTION_TYPE property_list = {},
                                                                WRENCH_MESSAGE_PAYLOADCOLLECTION_TYPE messagepayload_list = {});

        /***********************/
        /** \cond DEVELOPER   **/
        /***********************/

        virtual double getFileLastWriteDate(const std::shared_ptr<DataFile> &file, const std::string &path = "/") override;

        bool hasFile(const std::shared_ptr<FileLocation> &location) override;

        void removeDirectory(const std::string &path) override;
        void removeFile(const std::shared_ptr<FileLocation> &location) override;

        std::string getMountPoint() override;
        std::set<std::string> getMountPoints() override;
        bool hasMultipleMountPoints();
        bool hasMountPoint(const std::string &mp);

        double getTotalSpace() override;

        double getTotalFreeSpaceZeroTime() override;

        double getTotalFilesZeroTime() override;

        std::string getBaseRootPath() override;

        /**
         * @brief Reserve space at the storage service
         * @param location: a location
         * @return true if success, false otherwise
         */
        bool reserveSpace(std::shared_ptr<FileLocation> &location) override {
            try {
                std::string dot_file = location->getDotFilePath();
                if (this->reserved_space.find(dot_file) == this->reserved_space.end()) {
                    this->file_system->create_file(dot_file, (sg_size_t) location->getFile()->getSize());
                    this->reserved_space[location->getDotFilePath()] = this->file_system->open(dot_file, "w");
                }
            } catch (simgrid::Exception &e) {
                return false;
            }
            return true;
        }

        /**
         * @brief Unreserve space at the storage service
         * @param location: a location
         */
        void unreserveSpace(std::shared_ptr<FileLocation> &location) override {

            std::string dot_file = location->getDotFilePath();
            if (this->file_system->file_exists(dot_file)) {
                if (this->reserved_space.find(dot_file) != this->reserved_space.end()) {
                    this->reserved_space[dot_file]->close();
                    this->reserved_space.erase(dot_file);
                }
                this->file_system->unlink_file(dot_file);
            }
        }

//        /**
//        * @brief Get the mount point that stores a path
//        * @param path: path
//        *
//        * @return a mount point
//        */
//        std::string getPathMountPoint(const std::string &path) {
//            std::string mount_point, path_at_mount_point;
//            this->splitPath(path, mount_point, path_at_mount_point);
//            return mount_point;
//        }

        void createFile(const std::shared_ptr<FileLocation> &location) override;

        double getFileLastWriteDate(const std::shared_ptr<FileLocation> &location) override;

        simgrid::s4u::Disk *getDiskForPathOrNull(const std::string &path);

        /***********************/
        /** \endcond          **/
        /***********************/

        /***********************/
        /** \cond INTERNAL    **/
        /***********************/

        /**
         * @brief Determine whether the storage service is bufferized
         * @return true if bufferized, false otherwise
         */
        bool isBufferized() const override {
            return this->is_bufferized;
        }

        /**
         * @brief Determine the storage service's buffer size
         * @return a size in bytes
         */
        double getBufferSize() const override {
            return this->buffer_size;
        }


//        void decrementNumRunningOperationsForLocation(const std::shared_ptr<FileLocation> &location) override;
//
//        void incrementNumRunningOperationsForLocation(const std::shared_ptr<FileLocation> &location) override;
        /***********************/
        /** \endcond          **/
        /***********************/


    protected:

        friend class SimpleStorageServiceBufferized;
        friend class SimpleStorageServiceNonBufferized;

        /***********************/
        /** \cond INTERNAL    **/
        /***********************/
        SimpleStorageService(const std::string &hostname,
                             const std::set<std::string> &mount_points,
                             WRENCH_PROPERTY_COLLECTION_TYPE property_list,
                             WRENCH_MESSAGE_PAYLOADCOLLECTION_TYPE messagepayload_list,
                             const std::string &suffix);


        static unsigned long getNewUniqueNumber();

        /** @brief Maximum number of concurrent connections */
        unsigned long num_concurrent_connections;

        bool processStopDaemonRequest(S4U_CommPort *ack_commport);
        bool processFileDeleteRequest(const std::shared_ptr<FileLocation> &location,
                                      S4U_CommPort *answer_commport);
        bool processFileLookupRequest(const std::shared_ptr<FileLocation> &location,
                                      S4U_CommPort *answer_commport);
        bool processFreeSpaceRequest(S4U_CommPort *answer_commport,
                                     const std::string &path);


        /** @brief The service's buffer size */
        double buffer_size = 10000000;

        /** @brief Whether the service is bufferized */
        bool is_bufferized;

        /** @brief Retrieve the simple storage service's file system object **/
        std::shared_ptr<simgrid::fsmod::FileSystem> getFileSystem() {
            return this->file_system;
        }

        /** @brief File system */
        std::shared_ptr<simgrid::fsmod::FileSystem> file_system;

//        bool splitPath(const std::string &path, std::string &mount_point, std::string &path_at_mount_point);

        std::shared_ptr<FailureCause> validateFileReadRequest(const std::shared_ptr<FileLocation> &location);
        std::shared_ptr<FailureCause> validateFileWriteRequest(const std::shared_ptr<FileLocation> &location, double num_bytes_to_write, bool *file_already_there);
        std::shared_ptr<FailureCause> validateFileCopyRequest(const std::shared_ptr<FileLocation> &src_location, std::shared_ptr<FileLocation> &dst_location,  bool *dst_file_already_there);

        /***********************/
        /** \endcond          **/
        /***********************/

    private:
        friend class Simulation;

        void validateProperties();


#ifdef PAGE_CACHE_SIMULATION
        std::shared_ptr<MemoryManager> memory_manager;
#endif
    };

}// namespace wrench

#endif// WRENCH_SIMPLESTORAGESERVICE_H
