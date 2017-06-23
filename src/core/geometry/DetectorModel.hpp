/**
 * @file
 * @brief Base of detector models
 *
 * @copyright MIT License
 */

/**
 * @defgroup DetectorModels Detector models
 * @brief Collection of detector models supported by the framework
 */

#ifndef ALLPIX_DETECTOR_MODEL_H
#define ALLPIX_DETECTOR_MODEL_H

#include <array>
#include <string>
#include <utility>

#include <Math/Point2D.h>
#include <Math/Point3D.h>
#include <Math/Vector2D.h>
#include <Math/Vector3D.h>

#include "core/config/Configuration.hpp"
#include "core/config/exceptions.h"
#include "tools/ROOT.h"

#include "core/utils/log.h"

namespace allpix {
    /**
     * @ingroup DetectorModels
     * @brief Base of all detector models
     *
     * Implements the minimum required for a detector model. A model always has a pixel grid with a specific pixel size. The
     * pixel grid defines the base size of the sensor, chip and support. Excess length can be specified. Every part of the
     * detector model has a defined center and size which can be overloaded by specialized detector models. The basic
     * detector model also defines the rotation center in the local coordinate system.
     */
    class DetectorModel {
    public:
        /**
         * @brief Constructs the base detector model
         * @param config Configuration description of a model
         */
        explicit DetectorModel(const Configuration& config) : type_(config.getName()) {
            using namespace ROOT::Math;

            // Number of pixels
            setNPixels(config.get<DisplacementVector2D<Cartesian2D<int>>>("number_of_pixels"));
            // Size of the pixels
            setPixelSize(config.get<XYVector>("pixel_size"));

            // Sensor thickness
            setSensorThickness(config.get<double>("sensor_thickness"));
            // Excess around the sensor from the pixel grid
            auto default_sensor_excess = config.get<double>("sensor_excess", 0);
            setSensorExcessTop(config.get<double>("sensor_excess_top", default_sensor_excess));
            setSensorExcessBottom(config.get<double>("sensor_excess_bottom", default_sensor_excess));
            setSensorExcessLeft(config.get<double>("sensor_excess_left", default_sensor_excess));
            setSensorExcessRight(config.get<double>("sensor_excess_right", default_sensor_excess));

            // Chip thickness
            setChipThickness(config.get<double>("chip_thickness", 0));
            // Excess around the chip from the pixel grid
            auto default_chip_excess = config.get<double>("chip_excess", 0);
            setChipExcessTop(config.get<double>("chip_excess_top", default_chip_excess));
            setChipExcessBottom(config.get<double>("chip_excess_bottom", default_chip_excess));
            setChipExcessLeft(config.get<double>("chip_excess_left", default_chip_excess));
            setChipExcessRight(config.get<double>("chip_excess_right", default_chip_excess));

            // Support thickness
            setSupportThickness(config.get<double>("support_thickness", 0));
            // Excess around the support from the pixel grid
            auto default_support_excess = config.get<double>("support_excess", 0);
            setSupportExcessTop(config.get<double>("support_excess_top", default_support_excess));
            setSupportExcessBottom(config.get<double>("support_excess_bottom", default_support_excess));
            setSupportExcessLeft(config.get<double>("support_excess_left", default_support_excess));
            setSupportExcessRight(config.get<double>("support_excess_right", default_support_excess));
            // Support location
            auto support_location = config.get<std::string>("support_location", "chip");
            if(support_location != "sensor" && support_location != "chip") {
                throw InvalidValueError(config, "support_location", "location of the support should be 'chip' or 'sensor'");
            }
            setSupportLocation(support_location);
            // Support material
            setSupportMaterial(config.get<std::string>("support_material", "epoxy"));
        }
        /**
         * @brief Essential virtual destructor
         */
        virtual ~DetectorModel() = default;

        ///@{
        /**
         * @brief Use default copy and move behaviour
         */
        DetectorModel(const DetectorModel&) = default;
        DetectorModel& operator=(const DetectorModel&) = default;

        DetectorModel(DetectorModel&&) = default;
        DetectorModel& operator=(DetectorModel&&) = default;
        ///@}

        /**
         * @brief Get the type of the model
         * @return Model type
         */
        std::string getType() const { return type_; }

        /**
         * @brief Get local coordinate of the position and rotation center in global frame
         * @note It can be a counter intuitive that this is not usually the origin, neither the geometric center of the model
         *
         * The center coordinate corresponds to the \ref Detector::getPosition "position" in the global frame.
         */
        virtual ROOT::Math::XYZPoint getCenter() const {
            return ROOT::Math::XYZPoint(getSensorSize().x() / 2.0 - getPixelSize().x() / 2.0,
                                        getSensorSize().y() / 2.0 - getPixelSize().y() / 2.0,
                                        0);
        }

        /**
         * @brief Get size of the box around the model that contains all elements
         * @return Size of the detector model
         *
         * All elements should be covered by a box with \ref Detector::getCenter as center. This means that the size returned
         * by this method is likely larger than the minimum possible size of a box around all elements. It will only return
         * the minimum size if \ref Detector::getCenter corresponds with the geometric center of the model.
         */
        virtual ROOT::Math::XYZVector getSize() const {
            ROOT::Math::XYZVector max(std::numeric_limits<double>::lowest(),
                                      std::numeric_limits<double>::lowest(),
                                      std::numeric_limits<double>::lowest());
            ROOT::Math::XYZVector min(
                std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());

            std::array<ROOT::Math::XYZPoint, 3> centers = {{getSensorCenter(), getChipCenter(), getSupportCenter()}};
            std::array<ROOT::Math::XYZVector, 3> sizes = {{getSensorSize(), getChipSize(), getSupportSize()}};

            for(size_t i = 0; i < 3; ++i) {
                max.SetX(std::max(max.x(), (centers.at(i) + sizes.at(i) / 2.0).x()));
                max.SetY(std::max(max.y(), (centers.at(i) + sizes.at(i) / 2.0).y()));
                max.SetZ(std::max(max.z(), (centers.at(i) + sizes.at(i) / 2.0).z()));
                min.SetX(std::min(min.x(), (centers.at(i) - sizes.at(i) / 2.0).x()));
                min.SetY(std::min(min.y(), (centers.at(i) - sizes.at(i) / 2.0).y()));
                min.SetZ(std::max(min.z(), (centers.at(i) - sizes.at(i) / 2.0).z()));
            }

            ROOT::Math::XYZVector size;
            size.SetX(2 * std::max(max.x() - getCenter().x(), getCenter().x() - min.x()));
            size.SetY(2 * std::max(max.y() - getCenter().y(), getCenter().y() - min.y()));
            size.SetZ(2 * std::max(max.z() - getCenter().z(), getCenter().z() - min.z()));
            return size;
        }

        /* PIXEL GRID */
        /**
         * @brief Get number of pixel (replicated blocks in generic sensors)
         * @return Number of two dimensional pixels
         */
        virtual ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>> getNPixels() const {
            return number_of_pixels_;
        }
        /**
         * @brief Set number of pixels (replicated blocks in generic sensors)
         * @param val Number of two dimensional pixels
         */
        void setNPixels(ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>> val) {
            number_of_pixels_ = std::move(val);
        }
        /**
         * @brief Get size of a single pixel
         * @return Size of a pixel
         */
        virtual ROOT::Math::XYVector getPixelSize() const { return pixel_size_; }
        /**
         * @brief Set the size of a pixel
         * @param val Size of a pixel
         */
        void setPixelSize(ROOT::Math::XYVector val) { pixel_size_ = std::move(val); }
        /**
         * @brief Get total size of the pixel grid
         * @param val Size of the pixel grid
         *
         * @warning The grid has zero thickness
         * @note This is basically a 2D method, but provided in 3D because it is primarily used there
         */
        ROOT::Math::XYZVector getGridSize() const {
            return ROOT::Math::XYZVector(getNPixels().x() * getPixelSize().x(), getNPixels().y() * getPixelSize().y(), 0);
        }

        /* SENSOR */
        /**
         * @brief Get size of the sensor
         * @return Size of the sensor
         *
         * Calculated from \ref Detector::getGridSize "pixel grid size", sensor excess and sensor thickness
         */
        virtual ROOT::Math::XYZVector getSensorSize() const {
            ROOT::Math::XYZVector excess_thickness(
                (sensor_excess_[1] + sensor_excess_[3]), (sensor_excess_[0] + sensor_excess_[2]), sensor_thickness_);
            return getGridSize() + excess_thickness;
        }
        /**
         * @brief Get center of the sensor in local coordinates
         * @return Center of the sensor
         *
         * Center of the sensor with excess taken into account
         */
        virtual ROOT::Math::XYZPoint getSensorCenter() const {
            ROOT::Math::XYZVector offset(
                (sensor_excess_[1] - sensor_excess_[3]) / 2.0, (sensor_excess_[0] - sensor_excess_[2]) / 2.0, 0);
            return getCenter() + offset;
        }
        /**
         * @brief Set the thickness of the sensor
         * @param val Thickness of the sensor
         */
        void setSensorThickness(double val) { sensor_thickness_ = val; }
        /**
         * @brief Set the excess at the top of the sensor (positive y-coordinate)
         * @param val Sensor top excess
         */
        void setSensorExcessTop(double val) { sensor_excess_[0] = val; }
        /**
         * @brief Set the excess at the right of the sensor (positive x-coordinate)
         * @param val Sensor right excess
         */
        void setSensorExcessRight(double val) { sensor_excess_[1] = val; }
        /**
         * @brief Set the excess at the bottom of the sensor (negative y-coordinate)
         * @param val Sensor bottom excess
         */
        void setSensorExcessBottom(double val) { sensor_excess_[2] = val; }
        /**
         * @brief Set the excess at the left of the sensor (negative x-coordinate)
         * @param val Sensor right excess
         */
        void setSensorExcessLeft(double val) { sensor_excess_[3] = val; }

        /* CHIP */
        /**
         * @brief Get size of the chip
         * @return Size of the chip
         *
         * Calculated from \ref Detector::getGridSize "pixel grid size", chip excess and chip thickness
         */
        virtual ROOT::Math::XYZVector getChipSize() const {
            ROOT::Math::XYZVector excess_thickness(
                (chip_excess_[1] + chip_excess_[3]), (chip_excess_[0] + chip_excess_[2]), chip_thickness_);
            return getGridSize() + excess_thickness;
        }
        /**
         * @brief Get center of the chip in local coordinates
         * @return Center of the chip
         *
         * Center of the chip calculcated from chip excess and sensor offset
         */
        virtual ROOT::Math::XYZPoint getChipCenter() const {
            ROOT::Math::XYZVector offset((chip_excess_[1] - chip_excess_[3]) / 2.0,
                                         (chip_excess_[0] - chip_excess_[2]) / 2.0,
                                         getSensorSize().z() / 2.0 + getChipSize().z() / 2.0);
            return getCenter() + offset;
        }
        /**
         * @brief Set the thickness of the sensor
         * @param val Thickness of the sensor
         */
        void setChipThickness(double val) { chip_thickness_ = val; }
        /**
         * @brief Set the excess at the top of the chip (positive y-coordinate)
         * @param val Chip top excess
         */
        void setChipExcessTop(double val) { chip_excess_[0] = val; }
        /**
         * @brief Set the excess at the right of the chip (positive x-coordinate)
         * @param val Chip right excess
         */
        void setChipExcessRight(double val) { chip_excess_[1] = val; }
        /**
         * @brief Set the excess at the bottom of the chip (negative y-coordinate)
         * @param val Chip bottom excess
         */
        void setChipExcessBottom(double val) { chip_excess_[2] = val; }
        /**
         * @brief Set the excess at the left of the chip (negative x-coordinate)
         * @param val Chip left excess
         */
        void setChipExcessLeft(double val) { chip_excess_[3] = val; }

        /* SUPPORT */
        /**
         * @brief Get size of the support
         * @return Size of the support
         *
         * Calculated from \ref Detector::getGridSize "pixel grid size", chip excess and chip thickness
         */
        virtual ROOT::Math::XYZVector getSupportSize() const {
            ROOT::Math::XYZVector excess_thickness(
                (support_excess_[1] + support_excess_[3]), (support_excess_[0] + support_excess_[2]), support_thickness_);
            return getGridSize() + excess_thickness;
        }
        /**
         * @brief Get center of the support in local coordinates
         * @return Center of the support
         *
         * Center of the support calculcated from support excess, sensor and chip offsets
         */
        virtual ROOT::Math::XYZPoint getSupportCenter() const {
            ROOT::Math::XYZVector offset(
                (support_excess_[1] - support_excess_[3]) / 2.0, (support_excess_[0] - support_excess_[2]) / 2.0, 0);
            if(support_location_ == "sensor") {
                offset.SetZ(-getSensorSize().z() / 2.0 - getSupportSize().z() / 2.0);
            } else if(support_location_ == "chip") {
                offset.SetZ(getSensorSize().z() / 2.0 + getChipSize().z() + getSupportSize().z() / 2.0);
            }
            return getCenter() + offset;
        }
        /**
         * @brief Set the thickness of the support
         * @param val Thickness of the support
         */
        void setSupportThickness(double val) { support_thickness_ = val; }
        /**
         * @brief Set the excess at the top of the sensor (positive y-coordinate)
         * @param val Support top excess
         */
        void setSupportExcessTop(double val) { support_excess_[0] = val; }
        /**
         * @brief Set the excess at the right of the support (positive x-coordinate)
         * @param val Support right excess
         */
        void setSupportExcessRight(double val) { support_excess_[1] = val; }
        /**
         * @brief Set the excess at the bottom of the support (negative y-coordinate)
         * @param val Support bottom excess
         */
        void setSupportExcessBottom(double val) { support_excess_[2] = val; }
        /**
         * @brief Set the excess at the left of the support (negative x-coordinate)
         * @param val Support left excess
         */
        void setSupportExcessLeft(double val) { support_excess_[3] = val; }
        /**
         * @brief Set the location of the support (defaults to next to the chip)
         * @param val Location of the support ('chip' or 'sensor')
         */
        void setSupportLocation(std::string val) { support_location_ = std::move(val); };
        /**
         * @brief Get the support material
         * @return Name of the support material
         */
        std::string getSupportMaterial() { return support_material_; }
        /**
         * @brief Set the material of the support (defaults to epoxy)
         * @param val Name of the support material ('epoxy' or 'kapton' common)
         */
        // FIXME: define the set of allowed values more strictly
        void setSupportMaterial(std::string val) { support_material_ = std::move(val); };

    protected:
        std::string type_;

        ROOT::Math::DisplacementVector2D<ROOT::Math::Cartesian2D<int>> number_of_pixels_;
        ROOT::Math::XYVector pixel_size_;

        double sensor_thickness_{};
        double sensor_excess_[4]{};

        double chip_thickness_{};
        double chip_excess_[4]{};

        double support_thickness_{};
        double support_excess_[4]{};
        std::string support_location_;
        std::string support_material_;
    };
} // namespace allpix

#endif // ALLPIX_DETECTOR_MODEL_H
