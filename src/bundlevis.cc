// __BEGIN_LICENSE__
// 
// Copyright (C) 2008 United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration
// (NASA).  All Rights Reserved.
// 
// Copyright 2008 Carnegie Mellon University. All rights reserved.
// 
// This software is distributed under the NASA Open Source Agreement
// (NOSA), version 1.3.  The NOSA has been approved by the Open Source
// Initiative.  See the file COPYING at the top of the distribution
// directory tree for the complete NOSA document.
// 
// THE SUBJECT SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF ANY
// KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT
// LIMITED TO, ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL CONFORM TO
// SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
// A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT, ANY WARRANTY THAT
// THE SUBJECT SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY THAT
// DOCUMENTATION, IF PROVIDED, WILL CONFORM TO THE SUBJECT SOFTWARE.
//
// __END_LICENSE__

/// \file bundlevis.cc
///

#include "bundlevis.h"

#define LINE_LENGTH 1.0f
#define PROGRAM_NAME "Bundlevis v2.0"
#define PI 3.14159265

// This builds the 3 Axis that represents the camera
osg::Geode* build3Axis(void){
  osg::Geode* axisGeode = new osg::Geode();
  osg::Geometry* axis = new osg::Geometry();

  osg::Vec3Array* vertices = new osg::Vec3Array(6);
  (*vertices)[0].set( 0.0f, 0.0f, 0.0f );
  (*vertices)[1].set( 1.0f, 0.0f, 0.0f );
  (*vertices)[2].set( 0.0f, 0.0f, 0.0f );
  (*vertices)[3].set( 0.0f, 1.0f, 0.0f );
  (*vertices)[4].set( 0.0f, 0.0f, 0.0f );
  (*vertices)[5].set( 0.0f, 0.0f, 1.0f );
  axis->setVertexArray( vertices );

  osg::Vec4Array* colours = new osg::Vec4Array(6);
  (*colours)[0].set( 1.0f, 0.0f, 0.0f, 1.0f );
  (*colours)[1].set( 1.0f, 0.0f, 0.0f, 1.0f );
  (*colours)[2].set( 0.0f, 1.0f, 0.0f, 1.0f );
  (*colours)[3].set( 0.0f, 1.0f, 0.0f, 1.0f );
  (*colours)[4].set( 0.0f, 0.0f, 1.0f, 1.0f );
  (*colours)[5].set( 0.0f, 0.0f, 1.0f, 1.0f );
  axis->setColorArray( colours );
  axis->setColorBinding( osg::Geometry::BIND_PER_VERTEX );

  axis->addPrimitiveSet( new osg::DrawArrays( GL_LINES, 0, vertices->getNumElements()));

  osg::StateSet* stateSet = new osg::StateSet();
  stateSet->setAttribute( new osg::LineWidth(2) );

  axisGeode->setStateSet( stateSet );
  axisGeode->addDrawable( axis );

  return axisGeode;
}

// This builds the matrix transform for a camera
osg::MatrixTransform* CameraIter::buildMatrixTransform( const int& step ) {
  osg::MatrixTransform* mt = new osg::MatrixTransform;

  double ca = cos(_euler[step][0]), sa = sin(_euler[step][0]);
  double cb = cos(_euler[step][1]), sb = sin(_euler[step][1]);
  double cc = cos(_euler[step][2]), sc = sin(_euler[step][2]);

  osg::Matrix rot( cb*cc,           -cb*sc,          sb,        0,
	       sa*sb*cc+ca*sc,  -sa*sb*sc+ca*cc, -sa*cb,    0,
	       -ca*sb*cc+sa*sc, ca*sb*sc+sa*cc,  ca*cb,     0,
	       0,               0,               0,         1);

  rot.invert(rot);

  osg::Matrix trans( 1,   0,   0,   _position[step][0],
		     0,   1,   0,   _position[step][1],
		     0,   0,   1,   _position[step][2],
		     0,   0,   0,   1 );

  mt->setMatrix( trans * rot );

  mt->setUpdateCallback( new cameraMatrixCallback( this ));
  
  return mt;
}

// This will load a points iteration file
std::vector<PointIter*> loadPointData( std::string pntFile,
				       int* step ){

  std::cout << "Loading Points Iteration Data : " << pntFile << std::endl;

  // Determing the number of lines in the file
  std::ifstream file(pntFile.c_str(), std::ios::in);
  int numLines = 0;
  int numPoints = 0;
  int numTimeIter = 0;
  char c;
  while (!file.eof()){
    c=file.get();
    if (c == '\n'){
      int buffer;
      numLines++;
      file >> buffer;
      if ( buffer > numPoints )
	numPoints = buffer;
    }
  }
  numPoints++;
  numTimeIter=numLines/numPoints;

  std::cout << "Number of Points found: " << numPoints << std::endl;

  // Starting from the top again
  file.clear();
  file.seekg(0);

  // Building the point organizational structure
  std::vector<PointIter*> pointData;
  for ( int i = 0; i < numPoints; ++i )
    pointData.push_back( new PointIter(i, step) );

  // Loading up the data finally

  // For every time iteration
  for ( int t = 0; t < numTimeIter; ++t ){
    for ( int i = 0; i < numPoints; ++i ){
      float float_fill_buffer;
      osg::Vec3f vec_fill_buffer;

      // The first one is just the point number
      file >> float_fill_buffer;
      if (float_fill_buffer != i)
	std::cout << "Reading number mismatch. Reading pnt " << i 
		 << " , found it to be " << float_fill_buffer << std::endl;

      // Filling the vector
      file >> vec_fill_buffer[0];
      file >> vec_fill_buffer[1];
      file >> vec_fill_buffer[2];

      // Now attaching the data
      pointData[i]->addIteration( vec_fill_buffer,
				  0.0f);

    }
  }

  if ( pointData[0]->size() != (unsigned) numTimeIter )
    std::cout << "Number of Time Iterations found, " << numTimeIter 
	     << ", not equal loaded, " << pointData[0]->size()
             << std::endl;

  return pointData;
}

// This will load a camera data file
std::vector<CameraIter*> loadCameraData( std::string camFile,
					 int* step ){

  std::cout << "Loading Cameras Iteration Data : " << camFile << std::endl;

  // Determing the number of lines in the file
  std::ifstream file(camFile.c_str(), std::ios::in);
  int numLines = 0;
  int numCameras = 0;
  int numTimeIter = 0;
  int numCameraParam = 0;
  char c;
  int last_camera = 2000;
  while (!file.eof()){
    c=file.get();
    if (c == '\n') {
      int buffer;
      numLines++;
      file >> buffer;
      if (buffer > numCameras)
	numCameras = buffer;
      if (buffer != last_camera){
	last_camera = buffer;
	numCameraParam = 0;
      } else {
	numCameraParam++;
      }
    }
  }
  numCameras++;
  numTimeIter=numLines/(numCameras*numCameraParam);

  std::cout << "Number of Cameras found: " << numCameras << "\n"
	    << "Number of Camera Parameters found: " << numCameraParam << "\n"
            << "Number of Time Iterations found: " << numTimeIter << "\n";

  // Starting from the top again
  file.clear();
  file.seekg(0);

  // Building the camera organizational structure
  std::vector<CameraIter*> cameraData;
  for ( int j = 0; j < numCameras; ++j ) {
    if ( numCameraParam == 1 || numCameraParam == 6 )
      cameraData.push_back( new CameraIter(j, step) );
    else
      cameraData.push_back( new CameraIter(j, step, numCameraParam) );
  }

  // Loading up the data finally

  // For every time iteration
  std::string throwAway;
  for ( int t = 0; t < numTimeIter; ++t ){
    for ( int j = 0; j < numCameras; ++j ){
      float float_fill_buffer;
      
      if ( numCameraParam == 6 ) {         // This is a CAHVOR camera parameter
	
	//Right now I'm not going to worry myself with if the correct orientation is
	//calculated.. This will just get camera bore pointed in the right direction
	std::vector<osg::Vec3f> buffer;
	osg::Vec3f vec_fill_buffer;
	buffer.clear();

	for ( int p = 0; p < numCameraParam; ++p ){
	  
	  // First on is just the Camera ID
	  file >> float_fill_buffer;
	  if (float_fill_buffer != j)
	    std::cout << "Reading number mismatch. Reading camera " << j
		     << ", found it to be " << float_fill_buffer << std::endl;

	  if (!(file >> vec_fill_buffer[0])) {
	    file >> throwAway;
	    vec_fill_buffer[0] = 0;
	  }
	  if (!(file >> vec_fill_buffer[1])) {
	    file >> throwAway;
	    vec_fill_buffer[1] = 0;
	  }
	  if (!(file >> vec_fill_buffer[2])) {
	    file >> throwAway;
	    vec_fill_buffer[2] = 0;
	  }

	  buffer.push_back( vec_fill_buffer );

	}

	// Converting CAHVOR to euler angles
	vw::Vector3 pointing;
	for ( int p = 0; p < 3; ++p )
	  pointing[p] = buffer[1][p];

	double alpha = atan2( pointing[1] , pointing[0]) - PI/2;

	vw::Matrix<double, 3, 3> zrot;
	zrot(0,0) = cos(alpha);
	zrot(0,1) = -sin(alpha);
	zrot(1,0) = -zrot(0,1);
	zrot(1,1) = zrot(0,0);
	zrot(2,2) = 1;

	pointing = transpose(zrot)*pointing;
	
	double beta = atan2( pointing[2], pointing[1] ) - PI/2;

	vw::Matrix<double, 3, 3> xrot;
	xrot(0,0) = 1;
	xrot(1,1) = cos(beta);
	xrot(1,2) = -sin(beta);
	xrot(2,1) = -xrot(1,2);
	xrot(2,2) = xrot(1,1);

	vw::Matrix<double, 3, 3> rot_W2Cam = transpose(xrot) * transpose(zrot);
	

	// Calculating euler angles from matrix
	vec_fill_buffer[1] = asin( rot_W2Cam(0,2) );
	vec_fill_buffer[2] = acos( rot_W2Cam(0,0) / cos(vec_fill_buffer[1]) );

	// First check to see if correct
	if (abs(rot_W2Cam(0,1) - ( -cos(vec_fill_buffer[1])*sin( vec_fill_buffer[2] ) )) > 0.1) {
	  vec_fill_buffer[2] = asin( rot_W2Cam(0,1) / ( -1*cos(vec_fill_buffer[1]) ) );
	}

	vec_fill_buffer[0] = acos( rot_W2Cam(2,2)/cos(vec_fill_buffer[1]) );

	// Second check
	if (abs(rot_W2Cam(1,2) - ( -sin(vec_fill_buffer[0])*cos( vec_fill_buffer[1] ) )) > 0.1) {
	  vec_fill_buffer[0] = asin( rot_W2Cam(1,2)/( -1*cos(vec_fill_buffer[1]) ) );
	}

	// Third check
	double ca = cos(vec_fill_buffer[0]), sa = sin(vec_fill_buffer[0]);
	double cb = cos(vec_fill_buffer[1]), sb = sin(vec_fill_buffer[1]);
	double cc = cos(vec_fill_buffer[2]), sc = sin(vec_fill_buffer[2]);
	vw::Matrix<double, 3, 3> check;
	check(0,0) = cb*cc;
	check(0,1) = -cb*sc;
	check(0,2) = sb;
	check(1,0) = sa*sb*cc+ca*sc;
	check(1,1) = -sa*sb*sc+ca*cc;
	check(1,2) = -sa*cb;
	check(2,0) = -ca*sb*cc+sa*sc;
	check(2,1) = ca*sb*sc+sa*cc;
	check(2,2) = ca*cb;
	
	check = check - rot_W2Cam;
	double sum = 0;
	for ( int p1 = 0; p1 < 3; ++p1 )
	  for ( int p2 = 0; p2 < 3; ++p2 )
	    sum+=check(p1,p2);
	
	if (sum > 0.1) {
	  // I'm guessing beta was off by 180
	  vec_fill_buffer[1] = PI;
	  vec_fill_buffer[2] = acos(rot_W2Cam(0,0)/cos(vec_fill_buffer[1]));
	  // First check
	  if (abs(rot_W2Cam(0,1) - (-cos(vec_fill_buffer[1])*sin(vec_fill_buffer[2]))) > 0.1) {
	    vec_fill_buffer[2] = asin(rot_W2Cam(0,1)/(-cos(vec_fill_buffer[1])));
	  }
	  vec_fill_buffer[0] = acos( rot_W2Cam(2,2) / cos(vec_fill_buffer[1]) );
	  // Second check
	  if (abs(rot_W2Cam(1,2) - (-sin(vec_fill_buffer[0])*cos(vec_fill_buffer[1]))) > 0.1) {
	    vec_fill_buffer[0] = asin(rot_W2Cam(1,2)/(-cos(vec_fill_buffer[1])));
	  }
	}

	// Now attaching the data
	cameraData[j]->addIteration( buffer[0],
				     vec_fill_buffer );

      } else if ( numCameraParam == 1 ) {  // This is a EULER angle camera model
	
	// First one is just the Camera ID
	file >> float_fill_buffer;
	if (float_fill_buffer != j)
	  std::cout << "Reading number mismatch. Reading camera " << j
		   << ", found it to be " << float_fill_buffer << std::endl;

	osg::Vec3f vec_fill_buffer1, vec_fill_buffer2;

	// This whole conditional mess is to handle NAN, at least I
	// hope it works!
	if (!(file >> vec_fill_buffer1[0])) {
	  file >> throwAway;
	  vec_fill_buffer1[0] = 0;
	}
	if (!(file >> vec_fill_buffer1[1])) {
	  file >> throwAway;
	  vec_fill_buffer1[1] = 0;
	}
	if (!(file >> vec_fill_buffer1[2])) {
	  file >> throwAway;
	  vec_fill_buffer1[2] = 0;
	}
	if (!(file >> vec_fill_buffer2[0])) {
	  file >> throwAway;
	  vec_fill_buffer2[0] = 0;
	}
	if (!(file >> vec_fill_buffer2[1])) {
	  file >> throwAway;
	  vec_fill_buffer2[1] = 0;
	}
	if (!(file >> vec_fill_buffer2[2])) {
	  file >> throwAway;
	  vec_fill_buffer2[2] = 0;
	}

	// Now attaching the data
	cameraData[j]->addIteration( vec_fill_buffer1,
				     vec_fill_buffer2 );
      } else { // The data is probable a linescan camera .... need to add a new draw ability
	
	std::vector< osg::Vec3f> bufferPosition;
	std::vector< osg::Vec3f> bufferPose;
	osg::Vec3f vec_fill_buffer;
	bufferPosition.clear();
	bufferPose.clear();

	// Going through all the vertice data which was given
	for ( int p = 0; p < numCameraParam; ++p ) {
	  
	  // The first on is just the Camera ID
	  file >> float_fill_buffer;
	  if ( float_fill_buffer != j)
	    std::cout << "Reading number mismatch. Reading camera " << j
		      << ", found it to be " << float_fill_buffer << std::endl;

	  if (!(file >> vec_fill_buffer[0])) {
	    file >> throwAway;
	    vec_fill_buffer[0] = 0;
	  }
	  if (!(file >> vec_fill_buffer[1])) {
	    file >> throwAway;
	    vec_fill_buffer[1] = 0;
	  }
	  if (!(file >> vec_fill_buffer[2])) {
	    file >> throwAway;
	    vec_fill_buffer[2] = 0;
	  }

	  bufferPosition.push_back( vec_fill_buffer );

	  if (!(file >> vec_fill_buffer[0])) {
	    file >> throwAway;
	    vec_fill_buffer[0] = 0;
	  }
	  if (!(file >> vec_fill_buffer[1])) {
	    file >> throwAway;
	    vec_fill_buffer[1] = 0;
	  }
	  if (!(file >> vec_fill_buffer[2])) {
	    file >> throwAway;
	    vec_fill_buffer[2] = 0;
	  }

	  bufferPose.push_back( vec_fill_buffer );
	}

	// Cool.. now I'm going to attach this data 
	cameraData[j]->addIteration( bufferPosition, bufferPose );
      }
    }
  }

  if ( (cameraData[0]->size())/(cameraData[0]->getVertices()) != (unsigned) numTimeIter )
    std::cout << "Number of Time Iterations found, " << numTimeIter
	      << ", not equal loaded, " << cameraData[0]->size()/cameraData[0]->getVertices()
	     << std::endl;
  return cameraData;
}

// This will load a control network file
std::vector<ConnLineIter*> loadControlNet( std::string cnetFile,
					   std::vector<PointIter*>& points,
					   std::vector<CameraIter*>& cameras,
					   vw::camera::ControlNetwork* cnet,
					   int* step ) {
  std::cout << "Loading Control Network : " << cnetFile << std::endl;

  cnet = new vw::camera::ControlNetwork("Bundlevis");
  cnet->read_control_network( cnetFile );

  if ( cnet->size() != points.size() ) {
    std::cout << "Control network doesn't seem to match loaded points data. Number of points: "
	     << points.size() << " Number of points in Control Network: "
	     << cnet->size() << std::endl;
    delete cnet;
    std::vector<ConnLineIter*> blank;
    return blank;
  }

  // Building the Connecting Line organizational structure
  std::vector<ConnLineIter*> connLineData;
  // For every point
  for ( unsigned p = 0; p < cnet->size(); ++p ) {
    // For every measure
    for ( unsigned m = 0; m < (*cnet)[p].size(); ++m ){
      // Is this a valid camera ?
      if ( (*cnet)[p][m].image_id() < (signed)cameras.size() ) {
	// Now popping on a new connection
	connLineData.push_back( new ConnLineIter( points[p],
						  cameras[(*cnet)[p][m].image_id()],
						  step ) ); 
      }
    }
  }

  std::cout << "Number of Connections found: " << connLineData.size()
	    << std::endl;

  return connLineData;

}

// This will build the entire scene
osg::Node* createScene( std::vector<PointIter*>& points,
			std::vector<CameraIter*>& cameras,
			std::vector<ConnLineIter*>& connLines ) {
  
  osg::Group* scene = new osg::Group();

  // First, setting the back drop to be dark
  {
    osg::ClearNode* backdrop = new osg::ClearNode;
    backdrop->setClearColor( osg::Vec4f( 0.1f, 0.1f, 0.1f, 1.0f ) );
    scene->addChild(backdrop);
  }

  // Second, drawing the points
  if ( points.size() ){
    osg::Geometry* geometry = new osg::Geometry;

    // Setting up vertices
    osg::Vec3Array* vertices = new osg::Vec3Array( points.size()*2 );
    geometry->setVertexArray( vertices );

    for ( unsigned i = 0; i < points.size()*2; i+=2 ) {
      (*vertices)[i] = points[i/2]->getPosition(0);
      (*vertices)[i+1] = (*vertices)[i] + osg::Vec3f(0.0f, 0.0f, 0.1f);
    }

    // Setting up color
    osg::Vec4Array* colours = new osg::Vec4Array( 1 );
    geometry->setColorArray( colours );
    geometry->setColorBinding( osg::Geometry::BIND_OVERALL );
    (*colours)[0].set(1.0f, 0.5f, 1.0f, 1.0f);

    // Setting up primitive to draw line to last location
    geometry->addPrimitiveSet( new osg::DrawArrays(GL_LINES, 0, vertices->size() ));

    // Setting up to draw points at the front end of the line
    osg::DrawElementsUInt* pointsArray = new osg::DrawElementsUInt(GL_POINTS, points.size());
    geometry->addPrimitiveSet( pointsArray );
    for (unsigned i = 0; i < points.size(); ++i)
      (*pointsArray)[i] = i*2;

    geometry->setUseDisplayList(false);
    geometry->setDrawCallback( new pointsDrawCallback( &points ) );
    
    // Making geode
    osg::Geode* geode = new osg::Geode;
    geode->addDrawable( geometry );
    

    // Making the points large
    osg::StateSet* stateSet = new osg::StateSet();
    osg::Point* point = new osg::Point();
    point->setSize( 2.0f );
    stateSet->setAttribute( point );
    geode->setStateSet( stateSet );

    scene->addChild( geode );
  }

  // Thirdly, draw cameras
  if ( cameras.size() ){
    osg::Geode* the3Axis = build3Axis();
    osg::Group* camerasGroup = new osg::Group;

    for (unsigned j = 0; j < cameras.size(); ++j){
      osg::MatrixTransform* mt = cameras[j]->buildMatrixTransform( 0 );
      mt->addChild( the3Axis );
      camerasGroup->addChild( mt );
    }

    // 3b.) Draw potential pushbroom camera lines
    osg::Group* pushbroomGroup = new osg::Group;
    {
      for (unsigned j = 0; j < cameras.size(); ++j){
	if ( cameras[j]->getIsPushbroom() ) {
	  std::cout << "Holy Cow we have a pushbroom" << std::endl;
	  
	  osg::Geometry* geometry = new osg::Geometry;

	  // Setting up vertices
	  osg::Vec3Array* vertices = new osg::Vec3Array( cameras[j]->getVertices() );
	  geometry->setVertexArray( vertices );
	  
	  geometry->setUseDisplayList( false );
	  geometry->setDrawCallback( new pushbroomDrawCallback( cameras[j] ) );

	  for ( unsigned i = 0; i < vertices->size(); ++i ){
	    (*vertices)[i] = cameras[j]->getPosition( 0, i );
	  }

	  // Setting up color
	  osg::Vec4Array* colours = new osg::Vec4Array( 1 );
	  geometry->setColorArray( colours );
	  geometry->setColorBinding( osg::Geometry::BIND_OVERALL );
	  (*colours)[0].set(1.0f, 1.0f, 0.0f, 1.0f);

	  // Setting up primitive to draw lines
	  geometry->addPrimitiveSet( new osg::DrawArrays(GL_LINE_STRIP, 0, vertices->size() ));

	  // Making geode
	  osg::Geode* geode = new osg::Geode;
	  geode->addDrawable( geometry );
	  
	  // Setting the line width
	  osg::StateSet* stateSet = new osg::StateSet();
	  stateSet->setAttribute( new osg::LineWidth(2) );
	  geode->setStateSet( stateSet );

	  pushbroomGroup->addChild( geode );
	}
      }
      camerasGroup->addChild( pushbroomGroup );
    }

    scene->addChild( camerasGroup );
  }

  // 4.) Draw Connecting Lines
  if ( connLines.size() ){
    osg::Group* linesGroup = new osg::Group;

    for ( unsigned i = 0; i < connLines.size(); i+= 2 ){
      osg::Geometry* geometry = new osg::Geometry;
      
      osg::Vec3Array* vertices = new osg::Vec3Array( 2 );
      (*vertices)[0] = connLines[i]->getPoint()->getPosition( 0 );
      (*vertices)[1] = connLines[i]->getCamera()->getPosition( 0 );
      geometry->setVertexArray( vertices );

      osg::Vec4Array* colours = new osg::Vec4Array( 1 );
      geometry->setColorArray( colours );
      geometry->setColorBinding( osg::Geometry::BIND_OVERALL );
      (*colours)[0].set(0.8f, 0.8f, 0.8f, 1.0f);
      
      geometry->addPrimitiveSet( new osg::DrawArrays(GL_LINES, 0, 2) );
      
      geometry->setUseDisplayList( false );
      geometry->setDrawCallback( new linesDrawCallback( connLines[i] ));

      osg::Geode* geode = new osg::Geode;
      geode->addDrawable( geometry );
      linesGroup->addChild( geode );
    }

    scene->addChild( linesGroup );

  }

  // 5.) Build hit points, surfaces that the mouse intersector can use
  {
    osg::Group* hitTargetGroup = new osg::Group;

    // Targets for cameras
    if ( cameras.size() ) {
      // Building the target that all will use
      osg::Vec3Array* vertices = new osg::Vec3Array(4);
      (*vertices)[0].set( 1.0f, -1.0f, -0.1f );
      (*vertices)[1].set( 1.0f, 1.0f, -0.1f );
      (*vertices)[2].set( -1.0f, 1.0f, -0.1f );
      (*vertices)[3].set( -1.0f, -1.0f, -0.1f );
      osg::Vec4Array* colours = new osg::Vec4Array(1);
      (*colours)[0].set( 1.0f, 1.0f, 1.0f, 0.0f );

      for ( unsigned j = 0; j < cameras.size(); ++j ) {
	// Transform that does my bidding
	osg::AutoTransform* autoT = new osg::AutoTransform;
	autoT->setAutoRotateMode( osg::AutoTransform::ROTATE_TO_SCREEN );
	autoT->setAutoScaleToScreen( true );
	autoT->setMinimumScale( 0.5f );
	autoT->setMaximumScale( 4.0f );
	autoT->setPosition( cameras[j]->getPosition( 0 ) );
	autoT->setUpdateCallback( new cameraAutoMatrixCallback( cameras[j] ));
	
	// Geode that is the target
	osg::Geometry* geometry = new osg::Geometry;
	geometry->setVertexArray( vertices );
	geometry->setColorArray( colours );
	geometry->setColorBinding( osg::Geometry::BIND_OVERALL );
	geometry->addPrimitiveSet( new osg::DrawArrays( osg::PrimitiveSet::QUADS,
							0,
							4 ) );
	osg::Geode* geode = new osg::Geode;
	geode->setName( cameras[j]->getDescription() );
	geode->setUserData( cameras[j] );
	geode->addDrawable( geometry );
	autoT->addChild( geode );
	
	// Add some text, cause it's cool
	osgText::Text* text = new osgText::Text;
	std::ostringstream os;
	os << (j + 1);
	text->setCharacterSize( 2.0f );
	text->setText( os.str() );
	text->setAlignment( osgText::Text::CENTER_CENTER );
	geode->addDrawable( text );

	hitTargetGroup->addChild( autoT );
      }
    }

    // Targets for points
    if ( points.size() ) {
      // Building the target that all will use
      osg::Vec3Array* vertices = new osg::Vec3Array(4);
      (*vertices)[0].set( 0.3f, -0.3f, -0.1f );
      (*vertices)[1].set( 0.3f, 0.3f, -0.1f );
      (*vertices)[2].set( -0.3f, 0.3f, -0.1f );
      (*vertices)[3].set( -0.3f, -0.3f, -0.1f );
      osg::Vec4Array* colours = new osg::Vec4Array(1);
      (*colours)[0].set( 1.0f, 1.0f, 1.0f, 0.0f );

      for ( unsigned i = 0; i < points.size(); ++i ) {
	// Building an LOD so it shuts off when too far away
	osg::LOD* lod = new osg::LOD;
	lod->setCenterMode( osg::LOD::USE_BOUNDING_SPHERE_CENTER );
	lod->setRangeMode( osg::LOD::DISTANCE_FROM_EYE_POINT );
	lod->setCenter( points[i]->getPosition( 0 ) );
	
	// Transform that does my bidding
	osg::AutoTransform* autoT = new osg::AutoTransform;
	autoT->setAutoRotateMode( osg::AutoTransform::ROTATE_TO_SCREEN );
	autoT->setAutoScaleToScreen( true );
	autoT->setMinimumScale( 0.01 );
	autoT->setMaximumScale( 1.0f );
	autoT->setPosition( points[i]->getPosition( 0 ) );
	autoT->setUpdateCallback( new pointAutoMatrixCallback( points[i] ));
	lod->addChild( autoT, 0, 10 );

	// Geode that is the target
	osg::Geometry* geometry = new osg::Geometry;
	geometry->setVertexArray( vertices );
	geometry->setColorArray( colours );
	geometry->setColorBinding( osg::Geometry::BIND_OVERALL );
	geometry->addPrimitiveSet( new osg::DrawArrays( osg::PrimitiveSet::QUADS,
						       0,
						       4 ) );
	osg::Geode* geode =new osg::Geode;
	geode->setName( points[i]->getDescription() );
	geode->addDrawable( geometry );
	geode->setUserData( points[i] );
	autoT->addChild( geode );

	// Add some text
	osgText::Text* text = new osgText::Text;
	std::ostringstream os;
	os << (i + 1);
	text->setCharacterSize( 0.5f );
	text->setText( os.str() );
	text->setAlignment( osgText::Text::CENTER_CENTER );
	geode->addDrawable( text );
       
	

	hitTargetGroup->addChild( lod );
      }
    }

    scene->addChild ( hitTargetGroup );
  }

  return scene;
}

// This is the event handler, mouse, and keyboard.
bool AllEventHandler::handle( const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa ) {
  // Note: for some reason, you should only perform getEventType()
  // once.
  if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN){
    switch (ea.getKey()){
    case 'z':     //Step Backwards
      {
	int buffer = (*_step);
	buffer--;
	if ( buffer < 1 )
	  buffer = _numIter;
	(*_step) = buffer;
	break;
      }
    case 'x':     //Play
      {
	_playControl->setPlay();
	break;
      }
    case 'c':     //Pause
      {
	_playControl->setPause();
	break;
      }
    case 'v':     //Stop
      {
	_playControl->setStop();
	break;
      }
    case 'b':     //Step Forward
      {
	int buffer = (*_step);
	buffer++;
	if ( buffer > _numIter )
	  buffer = 1;
	(*_step) = buffer;
	break;
      }
    case '0':     //Show all steps
      {
	(*_step) = 0;
	break;
      }
    case '1':     //Move to the first frame
      {
	(*_step) = 1;
	break;
      }
    case '2':     //Move ro a place somewhere in between
      {
	(*_step) = (int)( (_numIter * 2) / 9 );
	break;
      }
    case '3':
      {
	(*_step) = (int)( (_numIter * 3) / 9 );
	break;
      }
    case '4':
      {
	(*_step) = (int)( (_numIter * 4) / 9 );
	break;
      }
    case '5':
      {
	(*_step) = (int)( (_numIter * 5) / 9 );
	break;
      }
    case '6':
      {
	(*_step) = (int)( (_numIter * 6) / 9 );
	break;
      }
    case '7':
      {
	(*_step) = (int)( (_numIter * 7) / 9 );
	break;
      }
    case '8':
      {
	(*_step) = (int)( (_numIter * 8) / 9 );
	break;
      }
    case '9':     //Move to the last frame
      {
	(*_step) = _numIter;
	break;
      }
    } 
  } else if ( ea.getEventType() == osgGA::GUIEventAdapter::DOUBLECLICK ) {
    // If there was a user keypress
    osgViewer::View* view = dynamic_cast< osgViewer::View* >(&aa);
    if (view)
      pick( view, ea );
  }

  return false;
}

void AllEventHandler::pick( osgViewer::View* view, const osgGA::GUIEventAdapter& ea ) {
  osgUtil::LineSegmentIntersector::Intersections intersections;

  float x = ea.getX();
  float y = ea.getY();

  if (view->computeIntersections( x, y, intersections )){

    //I only care about the first intersection event
    osgUtil::LineSegmentIntersector::Intersections::iterator hit = intersections.begin();

    if (!hit->nodePath.empty() && !(hit->nodePath.back()->getName().empty())){
      std::cout << hit->nodePath.back()->getName() << std::endl;

      // Now I'll determine if it's a point
      PointIter* point = dynamic_cast< PointIter* >( hit->nodePath.back()->getUserData() );
      if ( point ) {
	point->setDrawConnLines( !point->getDrawConnLines() );
	return;
      }

      // Well... crap, is it a camera?
      CameraIter* camera = dynamic_cast< CameraIter* >( hit->nodePath.back()->getUserData() );
      if ( camera ) {
	camera->setDrawConnLines( !camera->getDrawConnLines() );
	return;
      }
    }
  }
}

// Main Execution
int main(int argc, char* argv[]){

  //Variables to be used later in this section
  int* controlStep = new int(0);
  std::string camera_iter_file;
  std::string points_iter_file;
  std::string pixel_iter_file;
  std::string control_net_file;
  vw::camera::ControlNetwork* cnet = NULL;
  std::vector<PointIter*> pointData;
  std::vector<CameraIter*> cameraData;
  std::vector<ConnLineIter*> connLineData;
  PlaybackControl* playControl = new PlaybackControl(controlStep);

  //OpenSceneGraph Variable which are important overall
  osgViewer::Viewer viewer;
  osg::Group* root = new osg::Group();

  //This first half of the program is wholly devoted to setting up boost program options
  //Boost program options code
  po::options_description general_options("Your most awesome Options");
  general_options.add_options()
    ("camera-iteration-file,c",po::value<std::string>(&camera_iter_file),"Load the camera parameters for each iteration from a file")
    ("points-iteration-file,p",po::value<std::string>(&points_iter_file),"Load the 3d points parameters for each iteration from a file")
    ("pixel-iteration-file,x",po::value<std::string>(&pixel_iter_file),"Loads the pixel information data. Allowing for an illustration of the pixel data over time")
    ("control-network-file,n",po::value<std::string>(&control_net_file),"Loads the control network for point and camera relationship status. Camera and Point Iteration data is need before a control network file can be used.")
    ("fullscreen","Sets the Bundlevis to render with the entire screen, doesn't work so hot with dual screens.")
    ("help","Display this help message");

  po::variables_map vm;
  po::store(po::parse_command_line(argc,argv,general_options),vm);
  po::notify(vm);

  std::ostringstream usage;
  usage << "Usage: " << argv[0] << "[options] <filename> ... " << std::endl << std::endl;
  usage << general_options << std::endl;

  if (vm.count("help")){
    std::cout<<usage.str();
    return 1;
  }

  //////////////////////////////////////////////////////////
  //Did the User mess up?
  if (!vm.count("points-iteration-file") && !vm.count("camera-iteration-file")){
    std::cout << usage.str();
    return 1;
  }

  /////////////////////////////////////////////////////////
  //Loading up camera iteration data
  if (vm.count("camera-iteration-file")){
    cameraData = loadCameraData( camera_iter_file,
				 controlStep );
  }

  /////////////////////////////////////////////////////////
  //Loading up point iteration data
  if (vm.count("points-iteration-file")){
    pointData = loadPointData( points_iter_file,
			       controlStep );
  }

  //////////////////////////////////////////////////////////
  //Loading the control network
  if (vm.count("control-network-file") && (pointData.size()) && (cameraData.size())){
    connLineData = loadControlNet( control_net_file,
				   pointData,
				   cameraData,
				   cnet,
				   controlStep );
  }

  //////////////////////////////////////////////////////////
  //Setting up window if requested
  if ( !vm.count( "fullscreen" ) ){
    std::cout << "Building Window\n";
    
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 100;
    traits->y = 100;
    traits->width = 1280;
    traits->height = 1024;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;
    traits->windowName = PROGRAM_NAME;
    traits->vsync = true;
    traits->useMultiThreadedOpenGLEngine = false;
    traits->supportsResize = true;
    traits->useCursor = true;

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());

    osg::ref_ptr<osg::Camera> camera = new osg::Camera;
    camera->setGraphicsContext(gc.get());
    camera->setViewport(new osg::Viewport(0,0, traits->width, traits->height));
    GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
    camera->setDrawBuffer(buffer);
    camera->setReadBuffer(buffer);

    viewer.addSlave(camera.get());
  }  

  //////////////////////////////////////////////////////////
  //Building Scene
  {
    root->addChild( createScene( pointData,
				 cameraData,
				 connLineData ) );
    viewer.setCameraManipulator( new osgGA::TrackballManipulator() );
    viewer.setSceneData(root);
    int numIter;
    if ( pointData.size() > 0 )
      numIter = pointData[0]->size();
    else
      numIter = cameraData[0]->size();
    viewer.addEventHandler( new AllEventHandler( controlStep,
						 numIter,
						 playControl) );

    root->setUpdateCallback( new playbackNodeCallback ( controlStep,
							numIter,
							playControl) );
					  
  }

  //////////////////////////////////////////////////////////
  //Setting some global scene parameters
  {
    osg::StateSet* stateSet = new osg::StateSet();
    stateSet->setMode( GL_LIGHTING, osg::StateAttribute::OFF );
    stateSet->setMode( GL_BLEND, osg::StateAttribute::ON );
    root->setStateSet( stateSet );
  }

  viewer.realize();
  viewer.run();
  std::cout << "Ending\n";

  return 0;
}