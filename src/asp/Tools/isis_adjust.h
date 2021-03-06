// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// \file isis_adjust.h
///
/// Utility for performing bundle adjustment of ISIS3 cube files. This
/// is a highly experimental program and reading of the bundle
/// adjustment chapter is required before use of this program.

// Standard
#include <stdlib.h>
#include <iostream>
#include <iomanip>

// Boost
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

// Vision Workbench
#include <vw/Math.h>
#include <vw/InterestPoint.h>
#include <vw/Core/Debugging.h>
#include <vw/BundleAdjustment.h>

// Ames Stereo Pipeline
#include <asp/IsisIO.h>
#include <asp/Sessions/StereoSession.h>
#include <asp/Sessions/ISIS/StereoSessionIsis.h>

// ISIS Bundle Adjustment Model
// This is the Bundle Adjustment model
template <unsigned positionParam, unsigned poseParam>
class IsisBundleAdjustmentModel : public vw::ba::ModelBase< IsisBundleAdjustmentModel < positionParam,
                                                                                                        poseParam >,
                                                                            (positionParam+poseParam), 3>{
  typedef vw::Vector<double, positionParam+poseParam> camera_vector_t;
  typedef vw::Vector<double, 3> point_vector_t;

  std::vector< boost::shared_ptr<vw::camera::IsisAdjustCameraModel> > m_cameras;
  boost::shared_ptr<vw::ba::ControlNetwork> m_network;
  std::vector<camera_vector_t> a;
  std::vector<point_vector_t> b;
  std::vector<camera_vector_t> a_target;
  std::vector<point_vector_t> b_target;
  std::vector< std::string > m_files;
  int m_num_pixel_observations;
  float m_spacecraft_position_sigma;
  float m_spacecraft_pose_sigma;
  float m_gcp_scalar;

public:

  IsisBundleAdjustmentModel( std::vector< boost::shared_ptr< vw::camera::IsisAdjustCameraModel> > const& camera_models,
                             boost::shared_ptr<vw::ba::ControlNetwork> network,
                             std::vector< std::string > input_names,
                             float const& spacecraft_position_sigma,
                             float const& spacecraft_pose_sigma, float const& gcp_scalar ) :
  m_cameras( camera_models ), m_network(network), a( camera_models.size() ),
    b( network->size()), a_target( camera_models.size() ),
    b_target( network->size() ), m_files( input_names ),
    m_spacecraft_position_sigma(spacecraft_position_sigma),
    m_spacecraft_pose_sigma(spacecraft_pose_sigma), m_gcp_scalar(gcp_scalar) {

    // Compute the number of observations from the bundle.
    m_num_pixel_observations = 0;
    for (unsigned i = 0; i < network->size(); ++i)
      m_num_pixel_observations += (*network)[i].size();

    // Set up the A and B vectors, storing the initial values.
    for (unsigned j = 0; j < m_cameras.size(); ++j) {
      // I'm using what is already in the IsisAdjust camera file as
      // the orginal starting point for the problem. This way I can
      // nudge it with error and see what it is doing for debuging
      boost::shared_ptr<asp::BaseEquation> posF = m_cameras[j]->position_func();
      boost::shared_ptr<asp::BaseEquation> poseF = m_cameras[j]->pose_func();

      a[j] = camera_vector_t();
      // Setting new equations defined by a_j
      for (unsigned n = 0; n < posF->size(); ++n)
        a[j][n] = (*posF)[n];
      for (unsigned n = 0; n < poseF->size(); ++n)
        a[j][n + posF->size()] = (*poseF)[n];
      a_target[j] = a[j];
    }

    // Setting up B vectors
    for (unsigned i = 0; i < network->size(); ++i) {
      b[i] = (*m_network)[i].position();
      b_target[i] = b[i];
    }

    // Checking to see if this Control Network is compatible with
    // IsisBundleAdjustmentModel
    if ( !(*m_network)[0][0].is_pixels_dominant() )
      vw_out(vw::WarningMessage,"asp") << "WARNING: Control Network doesn't appear to be using pixels" << std::endl;

  }

  // Return a reference to the camera and point parameters.
  camera_vector_t A_parameters( int j ) const { return a[j]; }
  point_vector_t B_parameters( int i ) const { return b[i]; }
  void set_A_parameters(int j, camera_vector_t const& a_j) { a[j] = a_j; }
  void set_B_parameters(int i, point_vector_t const& b_i) { b[i] = b_i; }

  // Return Initial parameters. (Used by the bundle adjuster )
  camera_vector_t A_target( int j ) const { return a_target[j]; }
  point_vector_t B_target( int i ) const { return b_target[i]; }

  // Return general sizes
  unsigned num_cameras() const { return a.size(); }
  unsigned num_points() const { return b.size(); }
  unsigned num_pixel_observations() const { return m_num_pixel_observations; }

  // Return pixel observations -> supposedly used by Bundlevis
  // eventually i think
  unsigned num_observations_of_point ( const int& i ) const { return (*m_network)[i].size(); }
  unsigned corresponding_camera_for_measure( const int& i, const int& m ) const {
    return (*m_network)[i][m].image_id();
  }

  // Return the covariance of the camera parameters for camera j.
  inline vw::Matrix<double, (positionParam+poseParam), (positionParam+poseParam)> A_inverse_covariance ( unsigned /*j*/ ) const {
    vw::Matrix< double, (positionParam+poseParam), (positionParam+poseParam) > result;
    for ( unsigned i = 0; i <positionParam; ++i )
      result(i,i) = 1/pow(m_spacecraft_position_sigma,2);
    for ( unsigned i = positionParam; i < (positionParam+poseParam); ++i )
      result(i,i) = 1/pow(m_spacecraft_pose_sigma,2);
    return result;
  }

  // Return the covariance of the point parameters for point i.
  inline vw::Matrix<double, 3, 3> B_inverse_covariance ( unsigned i ) const {
    vw::Matrix< double, 3, 3> result;
    vw::Vector3 sigmas = (*m_network)[i].sigma();
    sigmas = m_gcp_scalar*sigmas;
    for ( unsigned u = 0; u < 3; ++u)
      result(u,u) = 1/(sigmas[u]*sigmas[u]);

    // It is assumed that the GCP is defined in a local tangent frame
    // (East-North-Up)
    float lon = atan2(b[i][1],b[i][0]);
    float clon = cos(lon);
    float slon = sin(lon);
    float radius = sqrt( b[i][0]*b[i][0] +
                         b[i][1]*b[i][1] +
                         b[i][2]*b[i][2] );
    float z_over_radius = b[i][2]/radius;
    float sqrt_1_minus = sqrt(1-z_over_radius*z_over_radius);
    vw::Matrix< double, 3, 3> ecef_to_local;
    ecef_to_local(0,0) = -slon;
    ecef_to_local(0,1) = clon;
    ecef_to_local(1,0) = -z_over_radius*clon;
    ecef_to_local(1,1) = -z_over_radius*slon;
    ecef_to_local(1,2) = sqrt_1_minus;
    ecef_to_local(2,0) = sqrt_1_minus*clon;
    ecef_to_local(2,1) = sqrt_1_minus*slon;
    ecef_to_local(2,2) = z_over_radius;

    // Rotatation matrix I want to use is ecef_to_local^T
    // How to rotate a covariance matrix = R*E*R^T

    // Rotating inverse covariance matrix
    result = transpose(ecef_to_local)*result*ecef_to_local;

    return result;
  }

  // This is for writing isis_adjust file for later
  void write_adjustment( int j, std::string const& filename ) const {
    std::ofstream ostr( filename.c_str() );

    write_equation( ostr, m_cameras[j]->position_func() );
    write_equation( ostr, m_cameras[j]->pose_func() );

    ostr.close();
  }

  std::vector< boost::shared_ptr< vw::camera::CameraModel > >
  adjusted_cameras() const {
    std::vector< boost::shared_ptr<vw::camera::CameraModel> > cameras;
    for ( unsigned i = 0; i < m_cameras.size(); i++) {
      cameras.push_back(boost::shared_dynamic_cast<vw::camera::CameraModel>(m_cameras[i]));
    }
    return cameras;
  }

  boost::shared_ptr< vw::camera::IsisAdjustCameraModel >
  adjusted_camera( int j ) const {
    // Adjusting position and pose equations
    boost::shared_ptr<asp::BaseEquation> posF = m_cameras[j]->position_func();
    boost::shared_ptr<asp::BaseEquation> poseF = m_cameras[j]->pose_func();

    // Setting new equations defined by a_j
    for (unsigned n = 0; n < posF->size(); ++n)
      (*posF)[n] = a[j][n];
    for (unsigned n = 0; n < poseF->size(); ++n)
      (*poseF)[n] = a[j][n + posF->size()];

    return m_cameras[j];
  }

  // Given the 'a' vector ( camera model paramters ) for the j'th
  // image, and the 'b' vector (3D point location ) for the i'th
  // point, return the location of b_i on imager j in
  // millimeters. !!Warning!! This gives data back in millimeters
  // which is different from most implementations.
  vw::Vector2 operator() ( unsigned i, unsigned j,
                           camera_vector_t const& a_j,
                           point_vector_t const& b_i ) const {

    // Loading equations
    boost::shared_ptr<asp::BaseEquation> posF = m_cameras[j]->position_func();
    boost::shared_ptr<asp::BaseEquation> poseF = m_cameras[j]->pose_func();

    // Applying new equation constants
    for (unsigned n = 0; n < posF->size(); ++n)
      (*posF)[n] = a_j[n];
    for (unsigned n = 0; n < poseF->size(); ++n)
      (*poseF)[n] = a_j[n + posF->size()];

    // Determine what time to use for the camera forward
    // projection. Sorry that this is a search, I don't have a better
    // idea. :/
    int m = 0;
    while ( (*m_network)[i][m].image_id() != int(j) )
      m++;
    if ( int(j) != (*m_network)[i][m].image_id() )
      vw_throw( vw::LogicErr() << "ISIS Adjust: Failed to find measure matching camera id." );

    // Performing the forward projection. This is specific to the
    // IsisAdjustCameraModel. The first argument is really just
    // passing the time instance to load up a pinhole model for.
    vw::Vector2 forward_projection =
      m_cameras[j]->point_to_pixel( b_i );

    // Giving back the pixel measurement.
    return forward_projection;
  }

  void parse_camera_parameters(camera_vector_t a_j,
                               vw::Vector3 &position_correction,
                               vw::Vector3 &pose_correction) const {
    position_correction = subvector(a_j, 0, 3);
    pose_correction = subvector(a_j, 3, 3);
  }

  // Errors on the image plane
  std::string image_unit() const { return "px"; }
  void image_errors( std::vector<double>& px_errors ) const {
    px_errors.clear();
    for (unsigned i = 0; i < m_network->size(); ++i )
      for (unsigned m = 0; m < (*m_network)[i].size(); ++m ) {
        int camera_idx = (*m_network)[i][m].image_id();
        vw::Vector2 px_error = (*m_network)[i][m].position() -
          (*this)(i, camera_idx, a[camera_idx],b[i]);
        px_errors.push_back(norm_2(px_error));
      }
  }

  // Errors for camera position
  void camera_position_errors( std::vector<double>& camera_position_errors ) const {
    camera_position_errors.clear();
    for (unsigned j=0; j < this->num_cameras(); ++j ) {
      // TODO: This needs to be compliant if the BA is using a
      // non-zero order equation
      vw::Vector3 position_initial = subvector(a_target[j],0,3);
      vw::Vector3 position_now = subvector(a[j],0,3);
      camera_position_errors.push_back(norm_2(position_initial-position_now));
    }
  }

  // Errors for camera pose
  void camera_pose_errors( std::vector<double>& camera_pose_errors ) const {
    camera_pose_errors.clear();
    for (unsigned j=0; j < this->num_cameras(); ++j ) {
      // TODO: This needs to be compliant if the BA is using a
      // non-zero order equation
      vw::Vector3 pose_initial = subvector(a_target[j],3,3);
      vw::Vector3 pose_now = subvector(a[j],3,3);
      camera_pose_errors.push_back(norm_2(pose_initial-pose_now));
    }
  }

  // Errors for gcp errors
  void gcp_errors( std::vector<double>& gcp_errors ) const {
    gcp_errors.clear();
    for (unsigned i=0; i < this->num_points(); ++i )
      if ((*m_network)[i].type() == vw::ba::ControlPoint::GroundControlPoint) {
        point_vector_t p1 = b_target[i];
        point_vector_t p2 = b[i];
        gcp_errors.push_back(norm_2(subvector(p1,0,3) - subvector(p2,0,3)));
      }
  }

  // Give access to the control network
  boost::shared_ptr<vw::ba::ControlNetwork> control_network() const {
    return m_network;
  }

  void bundlevis_cameras_append(std::string const& filename ) const {
    std::ofstream ostr(filename.c_str(),std::ios::app);
    for ( unsigned j = 0; j < this->num_cameras(); ++j ) {
      boost::shared_ptr<vw::camera::IsisAdjustCameraModel> camera =
        this->adjusted_camera(j);
      float center_sample = camera->samples()/2;

      for ( int i = 0; i < camera->lines(); i+=(camera->lines()/4) ) {
        vw::Vector3 position =
          camera->camera_center( vw::Vector2(center_sample,i) );
        vw::Quat pose =
          camera->camera_pose( vw::Vector2(center_sample,i) );
        ostr << std::setprecision(18) << j << "\t" << position[0] << "\t"
             << position[1] << "\t" << position[2] << "\t";
        ostr << pose[0] << "\t" << pose[1] << "\t"
             << pose[2] << "\t" << pose[3] << "\n";
      }
    }
  }

  void bundlevis_points_append(std::string const& filename ) const {
    std::ofstream ostr(filename.c_str(),std::ios::app);
    unsigned i = 0;
    BOOST_FOREACH( point_vector_t const& p, b ) {
      ostr << i++ << std::setprecision(18) << "\t" << p[0] << "\t"
           << p[1] << "\t" << p[2] << "\n";
    }
  }
};

template<typename In, typename Out, typename Pred>
Out copy_if(In first, In last, Out res, Pred Pr)
{
  while (first != last)
  {
    if (Pr(*first))
      *res++ = *first;
    ++first;
  }
  return res;
}

// This sifts out from a vector of strings, a listing of GCPs.  This
// should be useful for those programs who accept their data in a mass
// input vector.
bool IsGCP( std::string const& name ) {
  return boost::iends_with(name,".gcp");
}

template <class IContainT, class OContainT>
void sort_out_gcp( IContainT& input, OContainT& output ) {
  copy_if( input.begin(), input.end(),
           std::back_inserter(output), IsGCP );
  typename IContainT::iterator new_end =
    std::remove_if(input.begin(), input.end(), IsGCP);
  input.erase(new_end,input.end());
}

// This sifts out from a vector of strings, a listing of input CNET
// GCPs. This should be useful for those programs who accept their data
// in a mass input vector.
bool IsGCPCnet( std::string const& name ) {
  return boost::iends_with(name,".net") || boost::iends_with(name,".cnet");
}

template <class IContainT, class OContainT>
void sort_out_gcpcnets( IContainT& input, OContainT& output ) {
  copy_if( input.begin(), input.end(),
           std::back_inserter(output), IsGCPCnet );
  typename IContainT::iterator new_end =
    std::remove_if(input.begin(), input.end(), IsGCPCnet );
  input.erase(new_end,input.end());
}
