/**
 * @file
 * @brief Definition of [LCIOWriter] module
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#include <string>
#include <vector>

#include "core/config/Configuration.hpp"
#include "core/geometry/GeometryManager.hpp"
#include "core/messenger/Messenger.hpp"
#include "core/module/Module.hpp"

#include "objects/PixelHit.hpp"

#include <IO/LCWriter.h>

namespace allpix {
    /**
     * @ingroup Modules
     * @brief Module to write hit data to LCIO file
     *
     * Create LCIO file, compatible to EUTelescope analysis framework.
     */
    class LCIOWriterModule : public Module {
    public:
        /**
         * @brief Constructor for this unique module
         * @param config Configuration object for this module as retrieved from the steering file
         * @param messenger Pointer to the messenger object to allow binding to messages on the bus
         * @param geo_manager Pointer to the geometry manager, containing the detectors
         */
        LCIOWriterModule(Configuration config, Messenger* messenger, GeometryManager* geo_manager);

        ~LCIOWriterModule();

        /**
         * @brief Receive pixel hit messages, create lcio event, add hit collection and write event to file.
         */
        void run(unsigned int) override;

        /**
         * @brief Close the output file
         */
        void finalize() override;

    private:
        Configuration config_;
        std::vector<std::shared_ptr<PixelHitMessage>> pixel_messages_;
        IO::LCWriter* lcWriter_{};
        std::map<std::string, unsigned int> detectorIDs_;
        int pixelType_;
        std::string OutputCollectionName_;
        std::string DetectorName_;
    };
} // namespace allpix
