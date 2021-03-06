/**
 * @file
 * @brief Implementation of simple charge transfer module
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#include "SimpleTransferModule.hpp"

#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include "core/config/exceptions.h"
#include "core/geometry/HybridPixelDetectorModel.hpp"
#include "core/utils/log.h"
#include "core/utils/unit.h"
#include "tools/ROOT.h"

#include "objects/PixelCharge.hpp"

using namespace allpix;

SimpleTransferModule::SimpleTransferModule(Configuration config, Messenger* messenger, std::shared_ptr<Detector> detector)
    : Module(config, detector), config_(std::move(config)), messenger_(messenger), detector_(std::move(detector)) {
    // Enable parallelization of this module if multithreading is enabled
    enable_parallelization();

    // Set default value for the maximum depth distance to transfer
    config_.setDefault("max_depth_distance", Units::get(5.0, "um"));

    // Save detector model
    model_ = detector_->getModel();

    // Require propagated deposits for single detector
    messenger->bindSingle(this, &SimpleTransferModule::propagated_message_, MsgFlags::REQUIRED);
}

void SimpleTransferModule::run(unsigned int) {
    // Find corresponding pixels for all propagated charges
    LOG(TRACE) << "Transferring charges to pixels";
    unsigned int transferred_charges_count = 0;
    std::map<Pixel::Index, std::vector<const PropagatedCharge*>, pixel_cmp> pixel_map;
    for(auto& propagated_charge : propagated_message_->getData()) {
        auto position = propagated_charge.getLocalPosition();
        // Ignore if outside depth range of implant
        // FIXME This logic should be improved
        if(std::fabs(position.z() - (model_->getSensorCenter().z() + model_->getSensorSize().z() / 2.0)) >
           config_.get<double>("max_depth_distance")) {
            LOG(DEBUG) << "Skipping set of " << propagated_charge.getCharge() << " propagated charges at "
                       << propagated_charge.getLocalPosition() << " because their local position is not in implant range";
            continue;
        }

        // Find the nearest pixel
        auto xpixel = static_cast<int>(std::round(position.x() / model_->getPixelSize().x()));
        auto ypixel = static_cast<int>(std::round(position.y() / model_->getPixelSize().y()));

        // Ignore if out of pixel grid
        if(xpixel < 0 || xpixel >= model_->getNPixels().x() || ypixel < 0 || ypixel >= model_->getNPixels().y()) {
            LOG(DEBUG) << "Skipping set of " << propagated_charge.getCharge() << " propagated charges at "
                       << propagated_charge.getLocalPosition() << " because their nearest pixel (" << xpixel << "," << ypixel
                       << ") is outside the grid";
            continue;
        }
        Pixel::Index pixel_index(static_cast<unsigned int>(xpixel), static_cast<unsigned int>(ypixel));

        // Update statistics
        unique_pixels_.insert(pixel_index);
        transferred_charges_count += propagated_charge.getCharge();

        LOG(DEBUG) << "Set of " << propagated_charge.getCharge() << " propagated charges at "
                   << propagated_charge.getLocalPosition() << " brought to pixel " << pixel_index;

        // Add the pixel the list of hit pixels
        pixel_map[pixel_index].emplace_back(&propagated_charge);
    }

    // Create pixel charges
    LOG(TRACE) << "Combining charges at same pixel";
    std::vector<PixelCharge> pixel_charges;
    for(auto& pixel_index_charge : pixel_map) {
        unsigned int charge = 0;
        for(auto& propagated_charge : pixel_index_charge.second) {
            charge += propagated_charge->getCharge();
        }

        // Get pixel object from detector
        auto pixel = detector_->getPixel(pixel_index_charge.first.x(), pixel_index_charge.first.y());

        pixel_charges.emplace_back(pixel, charge, pixel_index_charge.second);
        LOG(DEBUG) << "Set of " << charge << " charges combined at " << pixel.getIndex();
    }

    // Writing summary and update statistics
    LOG(INFO) << "Transferred " << transferred_charges_count << " charges to " << pixel_map.size() << " pixels";
    total_transferred_charges_ += transferred_charges_count;

    // Dispatch message of pixel charges
    auto pixel_message = std::make_shared<PixelChargeMessage>(pixel_charges, detector_);
    messenger_->dispatchMessage(this, pixel_message);
}

void SimpleTransferModule::finalize() {
    // Print statistics
    LOG(INFO) << "Transferred total of " << total_transferred_charges_ << " charges to " << unique_pixels_.size()
              << " different pixels";
}
