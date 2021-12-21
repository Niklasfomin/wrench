/**
 * Copyright (c) 2017. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WRENCH_FILETRANSFERTHREAD_H
#define WRENCH_FILETRANSFERTHREAD_H

#include <string>
#include "wrench/simgrid_S4U_util/S4U_Daemon.h"
#include "wrench/services/Service.h"
#include "wrench/services/storage/StorageService.h"

namespace wrench {

    class DataFile;
    class SimulationTimestampFileCopyStart;

    /***********************/
    /** \cond INTERNAL     */
    /***********************/

    /** @brief A helper class that implements the concept of a communication
     *  thread that performs a file transfer
     */
    class FileTransferThread : public Service {

    public:



        FileTransferThread(std::string hostname,
                           std::shared_ptr<StorageService> parent,
                           std::shared_ptr<DataFile>file,
                           double num_bytes_to_transfer,
                           std::string src_mailbox,
                           std::shared_ptr<FileLocation> dst_location,
                           std::string answer_mailbox_if_read,
                           std::string answer_mailbox_if_write,
                           std::string answer_mailbox_if_copy,
                           unsigned long buffer_size);

        FileTransferThread(std::string hostname,
                           std::shared_ptr<StorageService> parent,
                           std::shared_ptr<DataFile>file,
                           double num_bytes_to_transfer,
                           std::shared_ptr<FileLocation> src_location,
                           std::string dst_mailbox,
                           std::string answer_mailbox_if_read,
                           std::string answer_mailbox_if_write,
                           std::string answer_mailbox_if_copy,
                           unsigned long buffer_size);

        FileTransferThread(std::string hostname,
                           std::shared_ptr<StorageService> parent,
                           std::shared_ptr<DataFile>file,
                           double num_bytes_to_transfer,
                           std::shared_ptr<FileLocation> src_location,
                           std::shared_ptr<FileLocation> dsg_location,
                           std::string answer_mailbox_if_read,
                           std::string answer_mailbox_if_write,
                           std::string answer_mailbox_if_copy,
                           unsigned long buffer_size);

        int main() override;
        void cleanup(bool has_returned_from_main, int return_value) override;


    private:

        std::shared_ptr<StorageService> parent;
        std::shared_ptr<DataFile>file;

        // Only one of these two is valid
        std::string src_mailbox;
        std::shared_ptr<FileLocation> src_location;

        // Only one of these two is valid
        std::string dst_mailbox;
        std::shared_ptr<FileLocation> dst_location;

        double num_bytes_to_transfer;

        std::string answer_mailbox_if_read;
        std::string answer_mailbox_if_write;
        std::string answer_mailbox_if_copy;
        unsigned long buffer_size;

        void receiveFileFromNetwork(std::shared_ptr<DataFile>file, std::string mailbox, std::shared_ptr<FileLocation> location);
        void sendLocalFileToNetwork(std::shared_ptr<DataFile>file, std::shared_ptr<FileLocation> location, double num_bytes_to_transfer, std::string mailbox);
        void downloadFileFromStorageService(std::shared_ptr<DataFile>file, std::shared_ptr<FileLocation> src_location, std::shared_ptr<FileLocation> dst_location);
        void copyFileLocally(std::shared_ptr<DataFile>file, std::shared_ptr<FileLocation> src_location, std::shared_ptr<FileLocation> dst_location);

    };

    /***********************/
    /** \endcond           */
    /***********************/
}

#endif //WRENCH_FILETRANSFERTHREAD_H

