/**
 * @file
 * @brief Implements the Geant4 geometry construction process
 * @remarks Code is based on code from Mathieu Benoit
 * @copyright Copyright (c) 2017 CERN and the Allpix Squared authors.
 * This software is distributed under the terms of the MIT License, copied verbatim in the file "LICENSE.md".
 * In applying this license, CERN does not waive the privileges and immunities granted to it by virtue of its status as an
 * Intergovernmental Organization or submit itself to any jurisdiction.
 */

#include "GeometryConstructionG4.hpp"

#include <memory>
#include <string>
#include <utility>

#include <G4Box.hh>
#include <G4LogicalVolume.hh>
#include <G4NistManager.hh>
#include <G4PVDivision.hh>
#include <G4PVPlacement.hh>
#include <G4Sphere.hh>
#include <G4StepLimiterPhysics.hh>
#include <G4SubtractionSolid.hh>
#include <G4ThreeVector.hh>
#include <G4Tubs.hh>
#include <G4UnionSolid.hh>
#include <G4UserLimits.hh>
#include <G4VSolid.hh>
#include <G4VisAttributes.hh>

#include "core/geometry/HybridPixelDetectorModel.hpp"
#include "core/module/exceptions.h"
#include "core/utils/log.h"
#include "tools/ROOT.h"
#include "tools/geant4.h"

#include "Parameterization2DG4.hpp"

using namespace allpix;

GeometryConstructionG4::GeometryConstructionG4(GeometryManager* geo_manager, Configuration config)
    : geo_manager_(geo_manager), config_(std::move(config)) {}

/**
 * @brief Version of std::make_shared that does not delete the pointer
 *
 * This version is needed because some pointers are deleted by Geant4 internally, but they are stored as std::shared_ptr in
 * the framework.
 */
template <typename T, typename... Args> static std::shared_ptr<T> make_shared_no_delete(Args... args) {
    return std::shared_ptr<T>(new T(args...), [](T*) {});
}

/**
 * First initializes all the materials. Then constructs the world from the internally calculated world size with a certain
 * margin. Finally builds all the individual detectors.
 */
G4VPhysicalVolume* GeometryConstructionG4::Construct() {
    // Initialize materials
    init_materials();

    // Set world material
    std::string world_material = config_.get<std::string>("world_material", "air");
    std::transform(world_material.begin(), world_material.end(), world_material.begin(), ::tolower);
    if(materials_.find(world_material) == materials_.end()) {
        throw InvalidValueError(config_, "world_material", "material does not exists, use 'air' or 'vacuum'");
    }

    world_material_ = materials_[world_material];
    LOG(TRACE) << "Material of world is " << world_material_->GetName();

    // Calculate world size
    ROOT::Math::XYZVector half_world_size;
    ROOT::Math::XYZPoint min_coord = geo_manager_->getMinimumCoordinate();
    ROOT::Math::XYZPoint max_coord = geo_manager_->getMaximumCoordinate();
    half_world_size.SetX(std::max(std::abs(min_coord.x()), std::abs(max_coord.x())));
    half_world_size.SetY(std::max(std::abs(min_coord.y()), std::abs(max_coord.y())));
    half_world_size.SetZ(std::max(std::abs(min_coord.z()), std::abs(max_coord.z())));

    // Calculate and apply margins to world size
    auto margin_percentage = config_.get<double>("world_margin_percentage", 0.1);
    auto minimum_margin = config_.get<ROOT::Math::XYZPoint>("world_minimum_margin", {0, 0, 0});
    double add_x = half_world_size.x() * margin_percentage;
    if(add_x < minimum_margin.x()) {
        add_x = minimum_margin.x();
    }
    double add_y = half_world_size.y() * margin_percentage;
    if(add_y < minimum_margin.y()) {
        add_y = minimum_margin.y();
    }
    double add_z = half_world_size.z() * margin_percentage;
    if(add_z < minimum_margin.z()) {
        add_z = minimum_margin.z();
    }
    half_world_size.SetX(half_world_size.x() + add_x);
    half_world_size.SetY(half_world_size.y() + add_y);
    half_world_size.SetZ(half_world_size.z() + add_z);

    LOG(DEBUG) << "World size is " << display_vector(2 * half_world_size, {"mm"});

    // Build the world
    auto world_box = std::make_shared<G4Box>("World", half_world_size.x(), half_world_size.y(), half_world_size.z());
    solids_.push_back(world_box);
    world_log_ = std::make_unique<G4LogicalVolume>(world_box.get(), world_material_, "World", nullptr, nullptr, nullptr);

    // Set the world to invisible in the viewer
    world_log_->SetVisAttributes(G4VisAttributes::GetInvisible());

    // Place the world at the center
    world_phys_ =
        std::make_unique<G4PVPlacement>(nullptr, G4ThreeVector(0., 0., 0.), world_log_.get(), "World", nullptr, false, 0);

    // Build all the detectors in the world
    build_detectors();

    return world_phys_.get();
}

/**
 * Initializes all the internal materials. The following materials are supported by this module:
 * - vacuum
 * - air
 * - silicon
 * - epoxy
 * - kapton
 * - solder
 */
void GeometryConstructionG4::init_materials() {
    G4NistManager* nistman = G4NistManager::Instance();

    // Build table of materials from database
    materials_["silicon"] = nistman->FindOrBuildMaterial("G4_Si");
    materials_["plexiglass"] = nistman->FindOrBuildMaterial("G4_PLEXIGLASS");
    materials_["kapton"] = nistman->FindOrBuildMaterial("G4_KAPTON");
    materials_["copper"] = nistman->FindOrBuildMaterial("G4_Cu");
    materials_["air"] = nistman->FindOrBuildMaterial("G4_AIR");

    // Create required elements:
    G4Element* H = new G4Element("Hydrogen", "H", 1., 1.01 * CLHEP::g / CLHEP::mole);
    G4Element* C = new G4Element("Carbon", "C", 6., 12.01 * CLHEP::g / CLHEP::mole);
    G4Element* O = new G4Element("Oxygen", "O", 8., 16.0 * CLHEP::g / CLHEP::mole);
    G4Element* Cl = new G4Element("Chlorine", "Cl", 17., 35.45 * CLHEP::g / CLHEP::mole);
    G4Element* Sn = new G4Element("Tin", "Sn", 50., 118.710 * CLHEP::g / CLHEP::mole);
    G4Element* Pb = new G4Element("Lead", "Pb", 82., 207.2 * CLHEP::g / CLHEP::mole);

    // Add vacuum
    materials_["vacuum"] = new G4Material("Vacuum", 1, 1.01 * CLHEP::g / CLHEP::mole, 0.0001 * CLHEP::g / CLHEP::cm3);

    // Create Epoxy material
    G4Material* Epoxy = new G4Material("Epoxy", 1.3 * CLHEP::g / CLHEP::cm3, 3);
    Epoxy->AddElement(H, 44);
    Epoxy->AddElement(C, 15);
    Epoxy->AddElement(O, 7);
    materials_["epoxy"] = Epoxy;

    // Create Carbon Fiber material:
    G4Material* CarbonFiber = new G4Material("CarbonFiber", 1.5 * CLHEP::g / CLHEP::cm3, 2);
    CarbonFiber->AddMaterial(Epoxy, 0.4);
    CarbonFiber->AddElement(C, 0.6);
    materials_["carbonfiber"] = CarbonFiber;

    // Create PCB G-10 material
    G4Material* GTen = new G4Material("G10", 1.7 * CLHEP::g / CLHEP::cm3, 3);
    GTen->AddMaterial(nistman->FindOrBuildMaterial("G4_SILICON_DIOXIDE"), 0.773);
    GTen->AddMaterial(Epoxy, 0.147);
    GTen->AddElement(Cl, 0.08);
    materials_["g10"] = GTen;

    // Create solder material
    G4Material* Solder = new G4Material("Solder", 8.4 * CLHEP::g / CLHEP::cm3, 2);
    Solder->AddElement(Sn, 0.63);
    Solder->AddElement(Pb, 0.37);
    materials_["solder"] = Solder;
}

void GeometryConstructionG4::build_detectors() {
    // Loop through all detectors and construct them
    std::vector<std::shared_ptr<Detector>> detectors = geo_manager_->getDetectors();
    LOG(TRACE) << "Building " << detectors.size() << " device(s)";

    for(auto& detector : detectors) {
        // Get pointer to the model of the detector
        auto model = detector->getModel();

        std::string name = detector->getName();
        LOG(DEBUG) << "Creating Geant4 model for " << name;

        LOG(DEBUG) << " Wrapper dimensions of model: " << display_vector(model->getSize(), {"mm", "um"});
        LOG(DEBUG) << " Center of the geometry parts relative to the origin:";

        // Create the wrapper box and logical volume
        auto wrapper_box = std::make_shared<G4Box>(
            "wrapper_" + name, model->getSize().x() / 2.0, model->getSize().y() / 2.0, model->getSize().z() / 2.0);
        solids_.push_back(wrapper_box);
        auto wrapper_log =
            make_shared_no_delete<G4LogicalVolume>(wrapper_box.get(), world_material_, "wrapper_" + name + "_log");
        detector->setExternalObject("wrapper_log", wrapper_log);

        // Get position and orientation
        G4ThreeVector posWrapper = toG4Vector(detector->getPosition());
        ROOT::Math::Rotation3D orientation = detector->getOrientation();
        std::vector<double> copy_vec(9);
        orientation.GetComponents(copy_vec.begin(), copy_vec.end());
        auto rotWrapper = std::make_shared<G4RotationMatrix>(copy_vec.data());
        detector->setExternalObject("rotation_matrix", rotWrapper);

        // Place the wrapper
        auto wrapper_phys = make_shared_no_delete<G4PVPlacement>(
            rotWrapper.get(), posWrapper, wrapper_log.get(), "wrapper_" + name + "_phys", world_log_.get(), false, 0, true);
        detector->setExternalObject("wrapper_phys", wrapper_phys);

        /* SENSOR
         * the sensitive detector is the part that collects the deposits
         */

        // Create the sensor box and logical volume
        auto sensor_box = std::make_shared<G4Box>("sensor_" + name,
                                                  model->getSensorSize().x() / 2.0,
                                                  model->getSensorSize().y() / 2.0,
                                                  model->getSensorSize().z() / 2.0);
        solids_.push_back(sensor_box);
        auto sensor_log =
            make_shared_no_delete<G4LogicalVolume>(sensor_box.get(), materials_["silicon"], "sensor_" + name + "_log");
        detector->setExternalObject("sensor_log", sensor_log);

        // Place the sensor box
        auto sensor_pos = toG4Vector(model->getSensorCenter() - model->getCenter());
        LOG(DEBUG) << "  - Sensor\t\t:\t" << display_vector(sensor_pos, {"mm", "um"});
        auto sensor_phys = make_shared_no_delete<G4PVPlacement>(
            nullptr, sensor_pos, sensor_log.get(), "sensor_" + name + "_phys", wrapper_log.get(), false, 0, true);
        detector->setExternalObject("sensor_phys", sensor_phys);

        // Create the pixel box and logical volume
        auto pixel_box = std::make_shared<G4Box>("pixel_" + name,
                                                 model->getPixelSize().x() / 2.0,
                                                 model->getPixelSize().y() / 2.0,
                                                 model->getSensorSize().z() / 2.0);
        solids_.push_back(pixel_box);
        auto pixel_log =
            make_shared_no_delete<G4LogicalVolume>(pixel_box.get(), materials_["silicon"], "pixel_" + name + "_log");
        detector->setExternalObject("pixel_log", pixel_log);

        // Create the parameterization for the pixel grid
        std::shared_ptr<G4VPVParameterisation> pixel_param =
            std::make_shared<Parameterization2DG4>(model->getNPixels().x(),
                                                   model->getPixelSize().x(),
                                                   model->getPixelSize().y(),
                                                   -model->getGridSize().x() / 2.0,
                                                   -model->getGridSize().y() / 2.0,
                                                   0);
        detector->setExternalObject("pixel_param", pixel_param);

        // WARNING: do not place the actual parameterization, only use it if we need it

        /* CHIP
         * the chip connected to the bumps bond and the support
         */

        // Construct the chips only if necessary
        if(model->getChipSize().z() > 1e-9) {
            // Create the chip box
            auto chip_box = std::make_shared<G4Box>("chip_" + name,
                                                    model->getChipSize().x() / 2.0,
                                                    model->getChipSize().y() / 2.0,
                                                    model->getChipSize().z() / 2.0);
            solids_.push_back(chip_box);

            // Create the logical volume for the chip
            auto chip_log =
                make_shared_no_delete<G4LogicalVolume>(chip_box.get(), materials_["silicon"], "chip_" + name + "_log");
            detector->setExternalObject("chip_log", chip_log);

            // Place the chip
            auto chip_pos = toG4Vector(model->getChipCenter() - model->getCenter());
            LOG(DEBUG) << "  - Chip\t\t:\t" << display_vector(chip_pos, {"mm", "um"});
            auto chip_phys = make_shared_no_delete<G4PVPlacement>(
                nullptr, chip_pos, chip_log.get(), "chip_" + name + "_phys", wrapper_log.get(), false, 0, true);
            detector->setExternalObject("chip_phys", chip_phys);
        }

        /*
         * SUPPORT
         * optional layers of support
         */
        auto supports_log = std::make_shared<std::vector<std::shared_ptr<G4LogicalVolume>>>();
        auto supports_phys = std::make_shared<std::vector<std::shared_ptr<G4PVPlacement>>>();
        int support_idx = 0;
        for(auto& layer : model->getSupportLayers()) {
            // Create the box containing the support
            auto support_box = std::make_shared<G4Box>("support_" + name + "_" + std::to_string(support_idx),
                                                       layer.getSize().x() / 2.0,
                                                       layer.getSize().y() / 2.0,
                                                       layer.getSize().z() / 2.0);
            solids_.push_back(support_box);

            std::shared_ptr<G4VSolid> support_solid = support_box;
            if(layer.hasHole()) {
                // NOTE: Double the hole size in the z-direction to ensure no fake surfaces are created
                auto hole_box = std::make_shared<G4Box>("support_" + name + "_hole_" + std::to_string(support_idx),
                                                        layer.getHoleSize().x() / 2.0,
                                                        layer.getHoleSize().y() / 2.0,
                                                        layer.getHoleSize().z());
                solids_.push_back(hole_box);

                G4Transform3D transform(G4RotationMatrix(), toG4Vector(layer.getHoleCenter() - layer.getCenter()));
                auto subtraction_solid =
                    std::make_shared<G4SubtractionSolid>("support_" + name + "_subtraction_" + std::to_string(support_idx),
                                                         support_box.get(),
                                                         hole_box.get(),
                                                         transform);
                solids_.push_back(subtraction_solid);
                support_solid = subtraction_solid;
            }

            // Create the logical volume for the support
            auto support_material_iter = materials_.find(layer.getMaterial());
            if(support_material_iter == materials_.end()) {
                throw ModuleError("Cannot construct a support layer of material '" + layer.getMaterial() + "'");
            }
            auto support_log = make_shared_no_delete<G4LogicalVolume>(
                support_solid.get(), materials_["epoxy"], "support_" + name + "_log_" + std::to_string(support_idx));
            supports_log->push_back(support_log);

            // Place the support
            auto support_pos = toG4Vector(layer.getCenter() - model->getCenter());
            LOG(DEBUG) << "  - Support\t:\t" << display_vector(support_pos, {"mm", "um"});
            auto support_phys =
                make_shared_no_delete<G4PVPlacement>(nullptr,
                                                     support_pos,
                                                     support_log.get(),
                                                     "support_" + name + "_phys_" + std::to_string(support_idx),
                                                     wrapper_log.get(),
                                                     false,
                                                     0,
                                                     true);
            supports_phys->push_back(support_phys);

            ++support_idx;
        }
        detector->setExternalObject("supports_log", supports_log);
        detector->setExternalObject("supports_phys", supports_phys);

        // Build the bump bonds only for hybrid pixel detectors
        auto hybrid_model = std::dynamic_pointer_cast<HybridPixelDetectorModel>(model);
        if(hybrid_model != nullptr) {
            /* BUMPS
            * the bump bonds connect the sensor to the readout chip
            */

            // Get parameters from model
            auto bump_height = hybrid_model->getBumpHeight();
            auto bump_sphere_radius = hybrid_model->getBumpSphereRadius();
            auto bump_cylinder_radius = hybrid_model->getBumpCylinderRadius();

            // Create the volume containing the bumps
            auto bump_box = std::make_shared<G4Box>("bump_box_" + name,
                                                    hybrid_model->getSensorSize().x() / 2.0,
                                                    hybrid_model->getSensorSize().y() / 2.0,
                                                    bump_height / 2.);
            solids_.push_back(bump_box);

            // Create the logical wrapper volume
            auto bumps_wrapper_log =
                make_shared_no_delete<G4LogicalVolume>(bump_box.get(), world_material_, "bumps_wrapper_" + name + "_log");
            detector->setExternalObject("bumps_wrapper_log", bumps_wrapper_log);

            // Place the general bumps volume
            G4ThreeVector bumps_pos = toG4Vector(hybrid_model->getBumpsCenter() - hybrid_model->getCenter());
            LOG(DEBUG) << "  - Bumps\t\t:\t" << display_vector(bumps_pos, {"mm", "um"});
            auto bumps_wrapper_phys = make_shared_no_delete<G4PVPlacement>(nullptr,
                                                                           bumps_pos,
                                                                           bumps_wrapper_log.get(),
                                                                           "bumps_wrapper_" + name + "_phys",
                                                                           wrapper_log.get(),
                                                                           false,
                                                                           0,
                                                                           true);
            detector->setExternalObject("bumps_wrapper_phys", bumps_wrapper_phys);

            // Create the individual bump solid
            auto bump_sphere = std::make_shared<G4Sphere>(
                "bumps_" + name + "_sphere", 0, bump_sphere_radius, 0, 360 * CLHEP::deg, 0, 360 * CLHEP::deg);
            solids_.push_back(bump_sphere);
            auto bump_tube = std::make_shared<G4Tubs>(
                "bumps_" + name + "_tube", 0., bump_cylinder_radius, bump_height / 2., 0., 360 * CLHEP::deg);
            solids_.push_back(bump_tube);
            auto bump = std::make_shared<G4UnionSolid>("bumps_" + name, bump_sphere.get(), bump_tube.get());
            solids_.push_back(bump);

            // Create the logical volume for the individual bumps
            auto bumps_cell_log =
                make_shared_no_delete<G4LogicalVolume>(bump.get(), materials_["solder"], "bumps_" + name + "_log");
            detector->setExternalObject("bumps_cell_log", bumps_cell_log);

            // Place the bump bonds grid
            std::shared_ptr<G4VPVParameterisation> bumps_param = std::make_shared<Parameterization2DG4>(
                hybrid_model->getNPixels().x(),
                hybrid_model->getPixelSize().x(),
                hybrid_model->getPixelSize().y(),
                -(hybrid_model->getNPixels().x() * hybrid_model->getPixelSize().x()) / 2.0 +
                    (hybrid_model->getBumpsCenter().x() - hybrid_model->getCenter().x()),
                -(hybrid_model->getNPixels().y() * hybrid_model->getPixelSize().y()) / 2.0 +
                    (hybrid_model->getBumpsCenter().y() - hybrid_model->getCenter().y()),
                0);
            detector->setExternalObject("bumps_param", bumps_param);

            std::shared_ptr<G4PVParameterised> bumps_param_phys =
                std::make_shared<ParameterisedG4>("bumps_" + name + "_phys",
                                                  bumps_cell_log.get(),
                                                  bumps_wrapper_log.get(),
                                                  kUndefined,
                                                  hybrid_model->getNPixels().x() * hybrid_model->getNPixels().y(),
                                                  bumps_param.get(),
                                                  false);
            detector->setExternalObject("bumps_param_phys", bumps_param_phys);
        }

        // ALERT: NO COVER LAYER YET

        LOG(TRACE) << " Constructed detector " << detector->getName() << " succesfully";
    }
}
