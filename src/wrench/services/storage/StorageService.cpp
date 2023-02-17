/**
 * Copyright (c) 2017-2021. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench/services/storage/StorageServiceProperty.h>
#include <wrench/services/storage/storage_helpers/LogicalFileSystem.h>
#include <wrench/exceptions/ExecutionException.h>
#include <wrench/logging/TerminalOutput.h>
#include <wrench/services/storage/StorageService.h>
#include <wrench/services/storage/compound/CompoundStorageService.h>
#include <wrench/services/compute/cloud/CloudComputeService.h>
#include "wrench/services/storage/StorageServiceMessage.h"
#include <wrench/services/storage/StorageServiceMessagePayload.h>
#include <wrench/simgrid_S4U_util/S4U_Mailbox.h>
#include <wrench/simulation/Simulation.h>
#include <wrench/simgrid_S4U_util/S4U_PendingCommunication.h>

#include <memory>

WRENCH_LOG_CATEGORY(wrench_core_storage_service, "Log category for Storage Service");

#define GB (1000.0 * 1000.0 * 1000.0)

namespace wrench {
    /**
     * @brief Constructor
     *
     * @param hostname: the name of the host on which the service should run
     * @param service_name: the name of the storage service
     *
     * @throw std::invalid_argument
     */
    StorageService::StorageService(const std::string &hostname,
                                   const std::string &service_name) : Service(hostname, service_name) {

        this->state = StorageService::UP;
        this->is_scratch = false;
    }



    /**
     * @brief Indicate that this storage service is a scratch service of a ComputeService
     */
    void StorageService::setScratch() {
        this->is_scratch = true;
    }


//    /**
//     * @brief Store a file at a particular location BEFORE the simulation is launched
//     *
//     * @param location: a file location
//     *
//     */
//    void StorageService::stageFile(const std::shared_ptr<FileLocation> &location) {
//        location->getStorageService()->stageFile(location->getFile(), location->getMountPoint(),
//                                                 location->getAbsolutePathAtMountPoint());
//    }
//
//    /**
//     * @brief Store a file at a particular mount point and directory BEFORE the simulation is launched
//     *
//     * @param file: a file
//     * @param mountpoint: a mount point
//     * @param path: a path at the mount point
//     */
//    void StorageService::stageFile(const std::shared_ptr<DataFile> &file, const std::string &mountpoint, std::string path) {
//        auto fs = this->file_systems[mountpoint].get();
//
//        try {
//            fs->stageFile(file, std::move(path));
//        } catch (std::exception &e) {
//            throw;
//        }
//    }

    /**
     * @brief Stop the service
     */
    void StorageService::stop() {
        // Just call the super class's method
        Service::stop();
    }

    /***************************************************************/
    /****** THESE FUNCTIONS PROVIDE AN OBJECT API FOR TALKING ******/
    /******     TO SPECIFIC STORAGE SERVERS NOT LOCATIONS     ******/
    /******    CONSTRUCTS LOCATION AND FORWARDS TO DAEMON    ******/
    /***************************************************************/



    /**
     * @brief Synchronously write a file to the storage service
     *
     * @param answer_mailbox: the mailbox on which to expect the answer
     * @param file: the file
     * @param path: the file path
     * @param wait_for_answer: whether to wait for the answer
     *
     * @throw ExecutionException
     */
    void StorageService::writeFile(simgrid::s4u::Mailbox *answer_mailbox,
                                   const std::shared_ptr<FileLocation> &location,
                                   bool wait_for_answer) {

        if (location == nullptr) {
            throw std::invalid_argument("StorageService::writeFile(): Invalid arguments");
        }

        this->assertServiceIsUp();

        S4U_Mailbox::putMessage(this->mailbox,
                                new StorageServiceFileWriteRequestMessage(
                                        answer_mailbox,
                                        simgrid::s4u::this_actor::get_host(),
                                        location,
                                        this->getMessagePayloadValue(
                                                StorageServiceMessagePayload::FILE_WRITE_REQUEST_MESSAGE_PAYLOAD)));

        // Wait for a reply
        std::shared_ptr<SimulationMessage> message;

        message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);

        if (auto msg = dynamic_cast<StorageServiceFileWriteAnswerMessage *>(message.get())) {
            // If not a success, throw an exception
            if (not msg->success) {
                throw ExecutionException(msg->failure_cause);
            }

            // Update buffer size according to which storage service actually answered.
            auto buffer_size = msg->buffer_size;

            if (buffer_size < 1) {
                // just wait for the final ack (no timeout!)
                message = S4U_Mailbox::getMessage(answer_mailbox);
                if (not dynamic_cast<StorageServiceAckMessage *>(message.get())) {
                    throw std::runtime_error("StorageService::writeFile(): Received an unexpected [" +
                                             message->getName() + "] message instead of final ack!");
                }

            } else {
                // Bufferized
                double remaining = file->getSize();
                while (remaining - buffer_size > DBL_EPSILON) {
                    S4U_Mailbox::putMessage(msg->data_write_mailbox,
                                            new StorageServiceFileContentChunkMessage(
                                                    file, buffer_size, false));
                    remaining -= buffer_size;
                }
                S4U_Mailbox::putMessage(msg->data_write_mailbox, new StorageServiceFileContentChunkMessage(
                        file, remaining, true));

                //Waiting for the final ack
                message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);
                if (not dynamic_cast<StorageServiceAckMessage *>(message.get())) {
                    throw std::runtime_error("StorageService::writeFile(): Received an unexpected [" +
                                             message->getName() + "] message instead of final ack!");
                }
            }

        } else {
            throw std::runtime_error("StorageService::writeFile(): Received a totally unexpected [" +
                                     message->getName() + "] message!");
        }
    }


    /***************************************************************/
    /****** EVERYTHING BELOW ARE INTERACTIONS WITH THE DAEMON ******/
    /***************************************************************/

    /**
     * @brief Synchronously asks the storage service for its free space capacity
     * @return The free space in bytes of each mount point, as a map
     *
     */
    double StorageService::getFreeSpace() {
        assertServiceIsUp();

        // Send a message to the daemon
        auto answer_mailbox = S4U_Daemon::getRunningActorRecvMailbox();
        S4U_Mailbox::putMessage(this->mailbox, new StorageServiceFreeSpaceRequestMessage(
                answer_mailbox,
                this->getMessagePayloadValue(
                        StorageServiceMessagePayload::FREE_SPACE_REQUEST_MESSAGE_PAYLOAD)));

        // Wait for a reply
        std::unique_ptr<SimulationMessage> message = nullptr;
        message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);

        if (auto msg = dynamic_cast<StorageServiceFreeSpaceAnswerMessage *>(message.get())) {
            return msg->free_space;
        } else {
            throw std::runtime_error("StorageService::getFreeSpace(): Unexpected [" + message->getName() + "] message");
        }
    }

    /**
     * @brief Asks the storage service whether it holds a file
     *
     * @param answer_mailbox: the answer mailbox to which the reply from the server should be sent
     * @param file: the file to lookup
     * @param path: the file path
     * @param wait_for_answer: whether this call should wait for an answer
     *
     * @return true if the file is present, false otherwise
     */
    bool StorageService::lookupFile(simgrid::s4u::Mailbox *answer_mailbox,
                                    const std::shared_ptr<FileLocation> &location,
                                    bool wait_for_answer) {
        if (!answer_mailbox or !location) {
            throw std::invalid_argument("StorageService::lookupFile(): Invalid nullptr arguments");
        }

        assertServiceIsUp(this->shared_from_this());

        // Send a message to the daemon
        S4U_Mailbox::putMessage(
                this->mailbox,
                new StorageServiceFileLookupRequestMessage(
                        answer_mailbox,
                        location,
                        this->getMessagePayloadValue(
                                StorageServiceMessagePayload::FILE_LOOKUP_REQUEST_MESSAGE_PAYLOAD)));

        if (wait_for_answer) {
            // Wait for a reply
            auto message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);

            if (auto msg = dynamic_cast<StorageServiceFileLookupAnswerMessage *>(message.get())) {
                return msg->file_is_available;
            } else {
                throw std::runtime_error("StorageService::lookupFile(): Unexpected [" + message->getName() + "] message (was expecting StorageServiceFileLookupAnswerMessage)");
            }
        }
    }


    /**
     * @brief Read a file from the storage service
     *
     * @param answer_mailbox: the mailbox on which to expect the answer
     * @param file: the file
     * @param num_bytes: number of bytes to read
     * @param path: the file path
     * @param wait_for_answer: whether to wait for the answer
     */
    void StorageService::readFile(simgrid::s4u::Mailbox *answer_mailbox,
                                  const std::shared_ptr<FileLocation> &location,
                                  double num_bytes,
                                  bool wait_for_answer) {

        if (!answer_mailbox or !location or (num_bytes < 0.0)) {
            throw std::invalid_argument("StorageService::readFile(): Invalid nullptr/0 arguments");
        }

        assertServiceIsUp(this->shared_from_this());

        try {
            S4U_Mailbox::putMessage(this->mailbox,
                                    new StorageServiceFileReadRequestMessage(
                                            answer_mailbox,
                                            simgrid::s4u::this_actor::get_host(),
                                            location,
                                            num_bytes,
                                            this->getMessagePayloadValue(
                                                    StorageServiceMessagePayload::FILE_READ_REQUEST_MESSAGE_PAYLOAD)));
        } catch (ExecutionException &e) {
            throw;
        }

        if (wait_for_answer) {
            // Wait for a reply
            std::unique_ptr<SimulationMessage> message = nullptr;

            try {
                message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);
            } catch (ExecutionException &e) {
                throw;
            }

            if (auto msg = dynamic_cast<StorageServiceFileReadAnswerMessage *>(message.get())) {

                // If it's not a success, throw an exception
                if (not msg->success) {
                    std::shared_ptr<FailureCause> cause = msg->failure_cause;
                    throw ExecutionException(cause);
                }

                if (msg->buffer_size < 1) {
                    // Non-Bufferized
                    // Just wait for the final ack (no timeout!)
                    message = S4U_Mailbox::getMessage(answer_mailbox);
                    if (not dynamic_cast<StorageServiceAckMessage *>(message.get())) {
                        throw std::runtime_error("StorageService::readFile(): Received an unexpected [" +
                                                 message->getName() + "] message (was expecting a StorageServiceAckMessage)!");
                    }
                } else {
                    // Otherwise, retrieve the file chunks until the last one is received
                    while (true) {
                        std::shared_ptr<SimulationMessage> file_content_message = nullptr;
                        try {
                            file_content_message = S4U_Mailbox::getMessage(msg->mailbox_to_receive_the_file_content);
                        } catch (ExecutionException &e) {
                            S4U_Mailbox::retireTemporaryMailbox(msg->mailbox_to_receive_the_file_content);
                            throw;
                        }

                        if (auto file_content_chunk_msg = dynamic_cast<StorageServiceFileContentChunkMessage *>(
                                file_content_message.get())) {
                            if (file_content_chunk_msg->last_chunk) {
                                S4U_Mailbox::retireTemporaryMailbox(msg->mailbox_to_receive_the_file_content);
                                break;
                            }
                        } else {
                            S4U_Mailbox::retireTemporaryMailbox(msg->mailbox_to_receive_the_file_content);
                            throw std::runtime_error("StorageService::readFile(): Received an unexpected [" +
                                                     file_content_message->getName() + "] message! (was expecting a StorageServiceFileContentChunkMessage)");
                        }
                    }

                    S4U_Mailbox::retireTemporaryMailbox(msg->mailbox_to_receive_the_file_content);

                    //Waiting for the final ack
                    message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);
                    if (not dynamic_cast<StorageServiceAckMessage *>(message.get())) {
                        throw std::runtime_error("StorageService::readFile(): Received an unexpected [" +
                                                 message->getName() + "] message! (was expecting a StorageServiceAckMessage)");
                    }
                }
            }
        }
    }


    /**
 * @brief Synchronously and sequentially read a set of files from storage services
 *
 * @param locations: a map of files to locations
 *
 * @throw std::runtime_error
 * @throw ExecutionException
 */
    void StorageService::readFiles(std::map<std::shared_ptr<DataFile>, std::shared_ptr<FileLocation>> locations) {
        StorageService::writeOrReadFiles(READ, std::move(locations));
    }

    /**
 * @brief Synchronously and sequentially upload a set of files from storage services
 *
 * @param locations: a map of files to locations
 *
 * @throw std::runtime_error
 * @throw ExecutionException
 */
    void StorageService::writeFiles(std::map<std::shared_ptr<DataFile>, std::shared_ptr<FileLocation>> locations) {
        StorageService::writeOrReadFiles(WRITE, std::move(locations));
    }

    /**
 * @brief Synchronously and sequentially write/read a set of files to/from storage services
 *
 * @param action: FileOperation::READ (download) or FileOperation::WRITE
 * @param locations: a map of files to locations
 *
 * @throw std::runtime_error
 * @throw ExecutionException
 */
    void StorageService::writeOrReadFiles(FileOperation action,
                                          std::map<std::shared_ptr<DataFile>, std::shared_ptr<FileLocation>> locations) {
        for (auto const &f: locations) {
            if ((f.first == nullptr) or (f.second == nullptr)) {
                throw std::invalid_argument("StorageService::writeOrReadFiles(): invalid argument");
            }
        }

        // Create a temporary sorted list of files so that the order in which files are read/written is deterministic!
        std::map<std::string, std::shared_ptr<DataFile>> sorted_files;
        for (auto const &f: locations) {
            sorted_files[f.first->getID()] = f.first;
        }

        for (auto const &f: sorted_files) {
            auto file = f.second;
            auto location = locations[file];

            if (action == READ) {
                WRENCH_INFO("Reading file %s from location %s",
                            file->getID().c_str(),
                            location->toString().c_str());
                StorageService::readFile(location);

                WRENCH_INFO("File %s read", file->getID().c_str());

            } else {
                WRENCH_INFO("Writing file %s to location %s",
                            file->getID().c_str(),
                            location->toString().c_str());
                StorageService::writeFile(location);

                WRENCH_INFO("File %s written", file->getID().c_str());
            }
        }
    }

    /**
     * @brief Delete a file on the storage service
     *
     * @param answer_mailbox: the answer mailbox to which the reply from the server should be sent
     * @param location: the location to delete
     * @param wait_for_answer: whether this call should
     */
    void StorageService::deleteFile(simgrid::s4u::Mailbox *answer_mailbox,
                                    const std::shared_ptr<FileLocation> &location,
                                    bool wait_for_answer) {
        if (!answer_mailbox or !location) {
            throw std::invalid_argument("StorageService::deleteFile(): Invalid nullptr arguments");
        }

        if (location->isScratch()) {
            throw std::invalid_argument("StorageService::deleteFile(): Cannot be called on a SCRATCH location");
        }

        assertServiceIsUp(this->shared_from_this());

        // Send a message to the storage service's daemon
        S4U_Mailbox::putMessage(this->mailbox,
                                new StorageServiceFileDeleteRequestMessage(
                                        answer_mailbox,
                                        location,
                                        this->getMessagePayloadValue(StorageServiceMessagePayload::FILE_DELETE_REQUEST_MESSAGE_PAYLOAD)));

        if (wait_for_answer) {

            // Wait for a reply
            std::unique_ptr<SimulationMessage> message = nullptr;

            message = S4U_Mailbox::getMessage(answer_mailbox, this->network_timeout);

            if (auto msg = dynamic_cast<StorageServiceFileDeleteAnswerMessage *>(message.get())) {
                // On failure, throw an exception
                if (!msg->success) {
                    throw ExecutionException(std::move(msg->failure_cause));
                }
                WRENCH_INFO("Deleted file at %s", location->toString().c_str());
            } else {
                throw std::runtime_error("StorageService::deleteFile(): Unexpected [" + message->getName() + "] message (was expecting StorageServiceFileDeleteAnswerMessage)");
            }
        }
    }

    /**
     * @brief Synchronously ask the storage service to read a file from another storage service
     *
     * @param src_location: the location where to read the file
     * @param dst_location: the location where to write the file
     *
     * @throw ExecutionException
     * @throw std::invalid_argument
     */
    void StorageService::copyFile(const std::shared_ptr<FileLocation> &src_location,
                                  const std::shared_ptr<FileLocation> &dst_location) {
        if ((src_location == nullptr) || (dst_location == nullptr)) {
            throw std::invalid_argument("StorageService::copyFile(): Invalid nullptr arguments");
        }
        if (src_location->getFile() != dst_location->getFile()) {
            throw std::invalid_argument("StorageService::copyFile(): src and dst locations should be for the same file");
        }

        assertServiceIsUp(src_location->getStorageService());
        assertServiceIsUp(dst_location->getStorageService());

        auto file = src_location->getFile();
        bool src_is_bufferized = src_location->getStorageService()->isBufferized();
        bool src_is_non_bufferized = not src_is_bufferized;
        bool dst_is_bufferized = dst_location->getStorageService()->isBufferized();
        bool dst_is_non_bufferized = not dst_is_bufferized;

        //        if (src_is_non_bufferized and dst_is_bufferized) {
        //            throw std::runtime_error("Cannot copy a file from a non-bufferized storage service to a bufferized storage service (not implemented, yet)");
        //        }

        simgrid::s4u::Mailbox *mailbox_to_contact;
        if (dst_is_non_bufferized and !(std::dynamic_pointer_cast<CompoundStorageService>(src_location->getStorageService()))) {
            mailbox_to_contact = dst_location->getStorageService()->mailbox;
        } else if (src_is_non_bufferized and !(std::dynamic_pointer_cast<CompoundStorageService>(dst_location->getStorageService()))) {
            mailbox_to_contact = src_location->getStorageService()->mailbox;
        } else {
            mailbox_to_contact = dst_location->getStorageService()->mailbox;
        }

        // Send a message to the daemon of the dst service
        auto answer_mailbox = S4U_Daemon::getRunningActorRecvMailbox();
        src_location->getStorageService()->simulation->getOutput().addTimestampFileCopyStart(Simulation::getCurrentSimulatedDate(), file,
                                                                                             src_location,
                                                                                             dst_location);

        S4U_Mailbox::putMessage(
                mailbox_to_contact,
                new StorageServiceFileCopyRequestMessage(
                        answer_mailbox,
                        src_location,
                        dst_location,
                        nullptr,
                        dst_location->getStorageService()->getMessagePayloadValue(
                                StorageServiceMessagePayload::FILE_COPY_REQUEST_MESSAGE_PAYLOAD)));

        // Wait for a reply
        std::unique_ptr<SimulationMessage> message = nullptr;

        message = S4U_Mailbox::getMessage(answer_mailbox);

        if (auto msg = dynamic_cast<StorageServiceFileCopyAnswerMessage *>(message.get())) {
            if (msg->failure_cause) {
                throw ExecutionException(std::move(msg->failure_cause));
            }
        } else {
            throw std::runtime_error("StorageService::copyFile(): Unexpected [" + message->getName() + "] message");
        }
    }

    /**
     * @brief Asynchronously ask for a file copy between two storage services
     *
     * @param answer_mailbox: the mailbox to which a notification message will be sent
     * @param src_location: the source location
     * @param dst_location: the destination location
     *
     * @throw ExecutionException
     * @throw std::invalid_argument
     *
     */
    void StorageService::initiateFileCopy(simgrid::s4u::Mailbox *answer_mailbox,
                                          const std::shared_ptr<FileLocation> &src_location,
                                          const std::shared_ptr<FileLocation> &dst_location) {
        if ((src_location == nullptr) || (dst_location == nullptr)) {
            throw std::invalid_argument("StorageService::initiateFileCopy(): Invalid nullptr arguments");
        }
        if (src_location->getFile() != dst_location->getFile()) {
            throw std::invalid_argument("StorageService::initiateFileCopy(): src and dst locations should be for the same file");
        }

        assertServiceIsUp(src_location->getStorageService());
        assertServiceIsUp(dst_location->getStorageService());

        auto file = src_location->getFile();
        bool src_is_bufferized = src_location->getStorageService()->isBufferized();
        bool src_is_non_bufferized = not src_is_bufferized;
        bool dst_is_bufferized = dst_location->getStorageService()->isBufferized();
        bool dst_is_non_bufferized = not dst_is_bufferized;

        //        if (src_is_non_bufferized and dst_is_bufferized) {
        //            throw std::runtime_error("Cannot copy a file from a non-bufferized storage service to a bufferized storage service (not implemented, yet)");
        //        }

        simgrid::s4u::Mailbox *mailbox_to_contact;
        if (dst_is_non_bufferized) {
            mailbox_to_contact = dst_location->getStorageService()->mailbox;
        } else if (src_is_non_bufferized) {
            mailbox_to_contact = src_location->getStorageService()->mailbox;
        } else {
            mailbox_to_contact = dst_location->getStorageService()->mailbox;
        }

        src_location->getStorageService()->simulation->getOutput().addTimestampFileCopyStart(Simulation::getCurrentSimulatedDate(), file,
                                                                                             src_location,
                                                                                             dst_location);

        // Send a message to the daemon on the dst location
        S4U_Mailbox::putMessage(
                mailbox_to_contact,
                new StorageServiceFileCopyRequestMessage(
                        answer_mailbox,
                        src_location,
                        dst_location,
                        nullptr,
                        dst_location->getStorageService()->getMessagePayloadValue(
                                StorageServiceMessagePayload::FILE_COPY_REQUEST_MESSAGE_PAYLOAD)));
    }



    /**
     * @brief Store a file at a particular mount point ex-nihilo. Doesn't notify a file registry service and will do nothing (and won't complain) if the file already exists
     * at that location.
     *
     * @param location: a file location, must be the same object as the function is invoked on
     *
     * @throw std::invalid_argument
     */
    void StorageService::createFile(const std::shared_ptr<FileLocation> &location) {
        if (location->getStorageService() != this->getSharedPtr<StorageService>()) {
            throw std::invalid_argument("StorageService::createFile(file,location) must be called on the same StorageService that the location uses");
        }
        stageFile(location->getFile(), location->getMountPoint(),
                  location->getAbsolutePathAtMountPoint());
    }

    /**
 * @brief Store a file at a particular mount point ex-nihilo. Doesn't notify a file registry service and will do nothing (and won't complain) if the file already exists
 * at that location.

*
* @param file: a file
* @param path: path to file
*
*/
    void StorageService::createFile(const std::shared_ptr<DataFile> &file, const std::string &path) {

        createFile(FileLocation::LOCATION(this->getSharedPtr<StorageService>(), path, file));
    }

    /**
 * @brief Store a file at a particular mount point ex-nihilo. Doesn't notify a file registry service and will do nothing (and won't complain) if the file already exists
 * at that location.
 * @param file: a file
 *
 */
    void StorageService::createFile(const std::shared_ptr<DataFile> &file) {

        createFile(FileLocation::LOCATION(this->getSharedPtr<StorageService>(), getMountPoint(), file));
    }

    /**
     * @brief Determines whether the storage service is bufferized
     * @return true if bufferized, false otherwise
     */
    bool StorageService::isBufferized() const {
        return this->buffer_size > 1;
    }

    /**
     * @brief Determines whether the storage service has the file. This doesn't simulate anything and is merely a zero-simulated-time data structure lookup.
     * If you want to simulate the overhead of querying the StorageService, instead use lookupFile().
     *
     * @param file a file
     *
     * @return true if the file is present, false otherwise
     */
    bool StorageService::hasFile(const shared_ptr<DataFile> &file) {
        return this->hasFile(file, this->getMountPoint());
    }


}// namespace wrench
