/**
 * @author Neal Gauvin <neal.gauvin@cern.ch>
 * @author John Idarraga <idarraga@cern.ch>
 * @author Koen Wolters <koen.wolters@cern.ch>
 */

#include "ReadGeoDescription.hpp"

#include <set>

#include "core/utils/exceptions.h"
#include "core/utils/log.h"

#include "core/config/ConfigReader.hpp"
#include "core/geometry/PixelDetectorModel.hpp"

// ROOT
#include <Math/Vector3D.h>
#include <Math/Vector2D.h>

// FIXME: agree on the units to use
#include "CLHEP/Units/SystemOfUnits.h"

using namespace allpix;
using namespace ROOT::Math;

ReadGeoDescription::ReadGeoDescription(std::string file_name) : models_() {
    std::ifstream file(file_name);
    if(!file.good()) {
        throw ModuleException("Geometry description file '" + file_name + "' not found");
    }

    ConfigReader reader(file, file_name);

    for(auto& config : reader.getConfigurations()) {
        models_[config.getName()] = parse_config(config);
    }
}

std::shared_ptr<PixelDetectorModel> ReadGeoDescription::parse_config(const Configuration& config) {
    std::string model_name = config.getName();
    std::shared_ptr<PixelDetectorModel> model = std::make_shared<PixelDetectorModel>(model_name);

    // pixel amount
    if(config.has("pixel_amount")) {
      XYVector vec = config.get<XYVector>("pixel_amount");
        model->SetNPixelsX(static_cast<int>(std::round(vec.x())));
        model->SetNPixelsY(static_cast<int>(std::round(vec.y())));
    }

    // size, positions and offsets
    if(config.has("pixel_size")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("pixel_size");
        model->SetPixSizeX(vec.x());
        model->SetPixSizeY(vec.y());
        model->SetPixSizeZ(vec.z()); // FIXME: useless
    }
    if(config.has("chip_size")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("chip_size");
        model->SetChipHX(vec.x());
        model->SetChipHY(vec.y());
        model->SetChipHZ(vec.z());
    }
    if(config.has("chip_position")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("chip_position");
        model->SetChipPosX(vec.x());
        model->SetChipPosY(vec.y());
        model->SetChipPosZ(vec.z());
    }
    if(config.has("chip_offset")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("chip_offset");
        model->SetChipOffsetX(vec.x());
        model->SetChipOffsetY(vec.y());
        model->SetChipOffsetZ(vec.z());
    }
    if(config.has("sensor_size")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("sensor_size");
        model->SetSensorHX(vec.x());
        model->SetSensorHY(vec.y());
        model->SetSensorHZ(vec.z());
    }
    if(config.has("sensor_position")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("sensor_position");
        model->SetSensorPosX(vec.x());
        model->SetSensorPosY(vec.y());
        model->SetSensorPosZ(vec.z());
    }
    if(config.has("pcb_size")) {
        XYZVector vec = CLHEP::um * config.get<XYZVector>("pcb_size");
        model->SetPCBHX(vec.x());
        model->SetPCBHY(vec.y());
        model->SetPCBHZ(vec.z());
    }

    // gr excess?
    if(config.has("sensor_gr_excess_htop")) {
        model->SetSensorExcessHTop(CLHEP::um * config.get<double>("sensor_gr_excess_htop"));
    }
    if(config.has("sensor_gr_excess_hbottom")) {
        model->SetSensorExcessHBottom(CLHEP::um * config.get<double>("sensor_gr_excess_hbottom"));
    }
    if(config.has("sensor_gr_excess_hleft")) {
        model->SetSensorExcessHLeft(CLHEP::um * config.get<double>("sensor_gr_excess_hleft"));
    }
    if(config.has("sensor_gr_excess_hright")) {
        model->SetSensorExcessHRight(CLHEP::um * config.get<double>("sensor_gr_excess_hright"));
    }

    // bump parameters
    if(config.has("bump_radius")) {
        model->SetBumpRadius(CLHEP::um * config.get<double>("bump_radius"));
    }
    if(config.has("bump_height")) {
        model->SetBumpHeight(CLHEP::um * config.get<double>("bump_height"));
    }
    if(config.has("bump_dr")) {
        model->SetBumpDr(CLHEP::um * config.get<double>("bump_dr"));
    }
    if(config.has("bump_offset")) {
        XYVector vec = CLHEP::um * config.get<XYVector>("bump_offset");
        model->SetBumpOffsetX(vec.x());
        model->SetBumpOffsetY(vec.y());
    }

    return model;
}

// Return detector model
// FIXME: should we throw an error if it does not exists
std::shared_ptr<PixelDetectorModel> ReadGeoDescription::getDetectorModel(const std::string& name) const {
    if(models_.find(name) == models_.end()) {
        return nullptr;
    }
    return models_.at(name);
}
