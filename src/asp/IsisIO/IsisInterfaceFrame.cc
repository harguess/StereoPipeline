// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


// ASP
#include <asp/IsisIO/IsisInterfaceFrame.h>

using namespace vw;
using namespace asp;
using namespace asp::isis;

// Constructor
IsisInterfaceFrame::IsisInterfaceFrame( std::string const& filename ) :
  IsisInterface(filename) {

  // Gutting Isis::Camera
  m_distortmap = m_camera->DistortionMap();
  m_focalmap   = m_camera->FocalPlaneMap();
  m_detectmap  = m_camera->DetectorMap();
  m_alphacube  = new Isis::AlphaCube( m_label );

  // Calculating Center (just once)
  double ipos[3];
  m_camera->InstrumentPosition(ipos);
  m_center[0] = ipos[0]*1000;
  m_center[1] = ipos[1]*1000;
  m_center[2] = ipos[2]*1000;

  // Calculating Pose (just once)
  std::vector<double> rot_inst = m_camera->InstrumentRotation()->Matrix();
  std::vector<double> rot_body = m_camera->BodyRotation()->Matrix();
  MatrixProxy<double,3,3> R_inst(&(rot_inst[0]));
  MatrixProxy<double,3,3> R_body(&(rot_body[0]));

  // Instrument Rotation = Spacecraft to Camera's Frame
  // Body Rotation = Spacecraft to World Frame
  m_pose = Quat(R_body*transpose(R_inst));
}

Vector2
IsisInterfaceFrame::point_to_pixel( Vector3 const& point ) const {
  Vector2 result;

  Vector3 look;
  look = normalize( point - m_center );
  std::vector<double> lookB_copy(3);
  lookB_copy[0] = look[0];
  lookB_copy[1] = look[1];
  lookB_copy[2] = look[2];
  lookB_copy = m_camera->BodyRotation()->J2000Vector(lookB_copy);
  lookB_copy = m_camera->InstrumentRotation()->ReferenceVector(lookB_copy);
  look[0] = lookB_copy[0];
  look[1] = lookB_copy[1];
  look[2] = lookB_copy[2];
  look = m_camera->FocalLength() * ( look / look[2] );

  // Back Projecting
  m_distortmap->SetUndistortedFocalPlane( look[0], look[1] );
  m_focalmap->SetFocalPlane( m_distortmap->FocalPlaneX(),
                             m_distortmap->FocalPlaneY() );
  m_detectmap->SetDetector( m_focalmap->DetectorSample(),
                            m_focalmap->DetectorLine() );
  result[0] = m_alphacube->BetaSample( m_detectmap->ParentSample() );
  result[1] = m_alphacube->BetaLine( m_detectmap->ParentLine() );

  return result-Vector2(1,1);
}

Vector3
IsisInterfaceFrame::pixel_to_vector( Vector2 const& pix ) const {
  Vector2 px = pix + Vector2(1,1);

  Vector3 result;

  px[0] = m_alphacube->AlphaSample(px[0]);
  px[1] = m_alphacube->AlphaLine(px[1]);
  m_detectmap->SetParent( px[0], px[1] );
  m_focalmap->SetDetector( m_detectmap->DetectorSample(),
                           m_detectmap->DetectorLine() );
  m_distortmap->SetFocalPlane( m_focalmap->FocalPlaneX(),
                               m_focalmap->FocalPlaneY() );
  result[0] = m_distortmap->UndistortedFocalPlaneX();
  result[1] = m_distortmap->UndistortedFocalPlaneY();
  result[2] = m_distortmap->UndistortedFocalPlaneZ();
  result = normalize( result );
  std::vector<double> look(3);
  look[0] = result[0];
  look[1] = result[1];
  look[2] = result[2];
  look = m_camera->InstrumentRotation()->J2000Vector(look);
  look = m_camera->BodyRotation()->ReferenceVector(look);
  result[0] = look[0];
  result[1] = look[1];
  result[2] = look[2];
  return result;
}

Vector3
IsisInterfaceFrame::camera_center( Vector2 const& /*pix*/ ) const {
  return m_center;
}

Quat
IsisInterfaceFrame::camera_pose( Vector2 const& /*pix*/ ) const {
  return m_pose;
}
