/**
 * @file
 * @brief Definition of default digitization module
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#ifndef ALLPIX_DEFAULT_DIGITIZER_MODULE_H
#define ALLPIX_DEFAULT_DIGITIZER_MODULE_H

#include <memory>
#include <random>
#include <string>

#include "core/config/Configuration.hpp"
#include "core/messenger/Messenger.hpp"
#include "core/module/Module.hpp"

#include "objects/PixelCharge.hpp"

#include <TH1D.h>

namespace allpix {
    /**
     * @ingroup Modules
     * @brief Module to simulate digitization of collected charges
     * @note This module supports parallelization
     *
     * This module provides a relatively simple simulation of the frontend electronics behavior. It simulates the
     * propagation of the signal of collected charges through the amplifier, comparator and ADC while adding electronics
     * noise and simulating the threshold as well as accounting for threshold dispersion and ADC noise.
     */
    class DefaultDigitizerModule : public Module {
    public:
        /**
         * @brief Constructor for this detector-specific module
         * @param config Configuration object for this module as retrieved from the steering file
         * @param messenger Pointer to the messenger object to allow binding to messages on the bus
         * @param detector Pointer to the detector for this module instance
         */
        DefaultDigitizerModule(Configuration config, Messenger* messenger, std::shared_ptr<Detector> detector);

        /**
         * @brief Initialize optional ROOT histograms
         */
        void init() override;

        /**
         * @brief Simulate digitization process
         */
        void run(unsigned int) override;

        /**
         * @brief Finalize and write optional histograms
         */
        void finalize() override;

    private:
        std::mt19937_64 random_generator_;

        Configuration config_;
        Messenger* messenger_;

        // Input message with the charges on the pixels
        std::shared_ptr<PixelChargeMessage> pixel_message_;

        // Statistics
        unsigned long long total_hits_{};

        // Output histograms
        TH1D *h_pxq, *h_pxq_noise, *h_thr, *h_pxq_thr, *h_pxq_adc_smear, *h_pxq_adc;
    };
} // namespace allpix

#endif /* ALLPIX_DEFAULT_DIGITIZER_MODULE_H */
