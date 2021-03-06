// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// \file orbitviz.cc
///

/************************************************************************
 *     File: orbitviz.cc
 ************************************************************************/
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#include <vw/Core.h>
#include <vw/Image.h>
#include <vw/Math.h>
#include <vw/FileIO.h>
#include <vw/Camera.h>
#include <vw/Stereo.h>
#include <vw/Cartography.h>
using namespace vw;
using namespace vw::camera;
using namespace vw::stereo;
using namespace vw::cartography;

#include <asp/Core/Macros.h>

#include <asp/Sessions.h>

#if defined(ASP_HAVE_PKG_ISISIO) && ASP_HAVE_PKG_ISISIO == 1
#include <asp/IsisIO/DiskImageResourceIsis.h>
#endif

struct Options {
  Options() : loading_image_camera_order(true) {}
  // Input
  std::vector<std::string> input_files;
  std::string stereo_session_string, path_to_outside_model;

  // Settings
  bool loading_image_camera_order;
  std::string datum;

  // Output
  std::string out_file;
};

// MAIN
//***********************************************************************
void handle_arguments( int argc, char *argv[], Options& opt ) {
  po::options_description general_options("");
  general_options.add_options()
    ("output,o", po::value(&opt.out_file)->default_value("orbit.kml"), "The output kml file that will be written")
    ("session-type,t", po::value(&opt.stereo_session_string), "Select the stereo session type to use for processing. [options: pinhole isis]")
    ("reference-spheroid,r", po::value(&opt.datum)->default_value("moon"), "Set a reference surface to a hard coded value (one of [moon, mars, wgs84].)")
    ("use_path_to_dae_model,u", po::value(&opt.path_to_outside_model), "Instead of using an icon to mark a camera, use a 3D model with extension .dae")
    ("help,h", "Display this help message");

  po::options_description positional("");
  positional.add_options()
    ("input-files", po::value(&opt.input_files) );

  po::positional_options_description positional_desc;
  positional_desc.add("input-files", -1);

  po::options_description all_options;
  all_options.add(general_options).add(positional);

  po::variables_map vm;
  try {
    po::store( po::command_line_parser( argc, argv ).options(all_options).positional(positional_desc).run(), vm );
    po::notify( vm );
  } catch (po::error &e ) {
    vw_throw( ArgumentErr() << "Error parsing input:\n\t"
              << e.what() << general_options );
  }

  std::ostringstream usage;
  usage << "Usage: " << argv[0] << " [options] <input image> <input camera model> <...and repeat...>\n";
  usage << "Note: All cameras and their images must be of the same session type. Camera models only can be used as input for stereo sessions pinhole and isis.\n\n";

  // Determining if feed only camera model
  if ( opt.input_files.size() == 1 )
    opt.loading_image_camera_order = false;
  else if ( opt.input_files.size() > 1 ) {
    std::string first_extension =
      opt.input_files[0].substr( opt.input_files[0].size()-4,4 );
    if ( boost::iends_with(opt.input_files[1], first_extension ) )
      opt.loading_image_camera_order = false;
  }

  if ( vm.count("help") || opt.input_files.size() == 0 ||
       (opt.loading_image_camera_order && opt.input_files.size() < 2) )
    vw_throw( ArgumentErr() << usage.str() << general_options );


  // Look up for session type based on file extensions
  if (opt.stereo_session_string.empty()) {
    int testing_i = 0;
    if ( opt.loading_image_camera_order )
      testing_i = 1;
    if ( boost::iends_with(opt.input_files[testing_i], ".cahvor") ||
         boost::iends_with(opt.input_files[testing_i], ".cahv") ||
         boost::iends_with(opt.input_files[testing_i], ".pin") ||
         boost::iends_with(opt.input_files[testing_i], ".tsai") ) {
      vw_out() << "\t--> Detected pinhole camera file\n";
      opt.stereo_session_string = "pinhole";
    } else if (boost::iends_with(opt.input_files[testing_i], ".cub") ) {
      vw_out() << "\t--> Detected ISIS cube file\n";
      opt.stereo_session_string = "isis";
    } else {
      vw_throw( ArgumentErr() << "\n\n******************************************************************\n"
                << "Could not determine stereo session type.   Please set it explicitly\n"
                << "using the -t switch.\n"
                << "******************************************************************\n\n" );
    }
  }

}

int main(int argc, char* argv[]) {

#if defined(ASP_HAVE_PKG_ISISIO) && ASP_HAVE_PKG_ISISIO == 1
  // Register the Isis file handler with the Vision Workbench
  // DiskImageResource system.
  DiskImageResource::register_file_type(".cub",
                                        DiskImageResourceIsis::type_static(),
                                        &DiskImageResourceIsis::construct_open,
                                        &DiskImageResourceIsis::construct_create);
#endif

#if defined(ASP_HAVE_PKG_ISISIO) && ASP_HAVE_PKG_ISISIO == 1
  StereoSession::register_session_type( "isis", &StereoSessionIsis::construct);
#endif

  Options opt;
  try {
    handle_arguments( argc, argv, opt );

      StereoSession* session = StereoSession::create(opt.stereo_session_string);

      // Data to be loaded
      unsigned no_cameras = opt.loading_image_camera_order ? opt.input_files.size()/2 : opt.input_files.size();
      vw_out() << "Number of cameras: " << no_cameras << std::endl;
      std::vector<std::string> camera_names(no_cameras);
      cartography::XYZtoLonLatRadFunctor conv_func;

      // Copying file names
      for (unsigned load_i = 0, read_i = 0; load_i < no_cameras;
           load_i++) {
        camera_names[load_i] = opt.input_files[read_i];
        read_i += opt.loading_image_camera_order ? 2 : 1;
      }

      // Create the KML file.
      KMLFile kml( opt.out_file, "orbitviz" );
      // Style listing
      if ( opt.path_to_outside_model.empty() ) {
        // Placemark Style
        kml.append_style( "plane", "", 1.2,
                          "http://maps.google.com/mapfiles/kml/shapes/airports.png");
        kml.append_style( "plane_highlight", "", 1.4,
                          "http://maps.google.com/mapfiles/kml/shapes/airports.png");
        kml.append_stylemap( "camera_placemark", "plane",
                             "plane_highlight" );
      }

      // Load up datum
      cartography::Datum datum;
      if ( opt.datum == "mars" ) {
	datum.set_well_known_datum("D_MARS");
      } else if ( opt.datum == "moon" ) {
	datum.set_well_known_datum("D_MOON");
      } else if ( opt.datum == "wgs84" ) {
	datum.set_well_known_datum("WGS84");
      } else {
	vw_out() << "Unknown spheriod request: " << opt.datum << "\n";
	vw_out() << "->  Defaulting to WGS84\n";
	datum.set_well_known_datum("WGS84");
      }

      // Building Camera Models and then writing to KML
      for (unsigned load_i = 0, read_i = 0; load_i < no_cameras;
           load_i++) {
        boost::shared_ptr<camera::CameraModel> current_camera;
        if (opt.loading_image_camera_order) {
          current_camera = session->camera_model( opt.input_files[read_i],
                                                  opt.input_files[read_i+1] );
          read_i+=2;
        } else {
          current_camera = session->camera_model( opt.input_files[read_i],
                                                  opt.input_files[read_i] );
          read_i++;
        }

        // Adding Placemarks
        Vector3 lon_lat_alt = conv_func(current_camera->camera_center(Vector2()));
	lon_lat_alt[2] -= datum.radius(lon_lat_alt[0], lon_lat_alt[1]);

        if (!opt.path_to_outside_model.empty())
          kml.append_model( opt.path_to_outside_model,
                            lon_lat_alt.x(), lon_lat_alt.y(),
                            inverse(current_camera->camera_pose(Vector2())),
                            camera_names[load_i], "",
                            lon_lat_alt[2], 1 );
        else {
          kml.append_placemark( lon_lat_alt.x(), lon_lat_alt.y(),
                                camera_names[load_i], "", "camera_placemark",
                                lon_lat_alt[2], true );
        }

        // Note to future programmer:
        // 6371e3 meters is the radius of earth. Everything in GE appears to measured against this radius
      }

      kml.close_kml();
  } ASP_STANDARD_CATCHES;

  return 0;
}
