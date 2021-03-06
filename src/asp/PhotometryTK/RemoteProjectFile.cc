// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


#include <asp/PhotometryTK/RemoteProjectFile.h>
using namespace vw;
using namespace asp::pho;
using namespace vw::platefile;

// A dummy method for passing to the RPC calls below.
static void null_closure() {}

// Constructor
asp::pho::RemoteProjectFile::RemoteProjectFile( std::string const& url ) {
  std::string hostname, exchange, ptk_name;
  int port;
  asp::pho::parse_url( url, hostname, port, exchange, ptk_name );
  m_projectname = ptk_name;

  std::string queue_name =
    AmqpRpcClient::UniqueQueueName("remote_ptk_"+ptk_name);

  boost::shared_ptr<AmqpConnection> conn(new AmqpConnection(hostname,port));
  m_rpc_controller.reset(new AmqpRpcClient(conn,exchange,queue_name,"ptk"));
  m_ptk_service.reset(new ProjectService::Stub( m_rpc_controller.get()));
  m_rpc_controller->bind_service( m_ptk_service, queue_name );

  { // Finding out project ID
    ProjectOpenRequest request;
    request.set_name( ptk_name );
    ProjectOpenReply response;
    m_ptk_service->OpenRequest( m_rpc_controller.get(), &request, &response,
                                google::protobuf::NewCallback(&null_closure) );
    if ( response.project_id() < 0 )
      vw_throw( ArgumentErr() << "Unable to open remote project file. Check url.\n" );
    m_project_id = response.project_id();
  }
}

// Error checking macros
#define CHECK_PROJECT_ID() \
  VW_ASSERT( response.project_id() == m_project_id, \
             IOErr() << "Unable to open remote project file" );
#define CHECK_CAMERA_ID(a) \
  VW_ASSERT( response.camera_id() == a, \
             ArgumentErr() << "Requested unavailable camera ID: " << a );

// Other public interface
void asp::pho::RemoteProjectFile::OpenProjectMeta( ProjectMeta& meta ) {
  ProjectOpenRequest request;
  request.set_name( m_projectname );
  ProjectOpenReply response;
  m_ptk_service->OpenRequest( m_rpc_controller.get(), &request, &response,
                              google::protobuf::NewCallback(&null_closure) );
  CHECK_PROJECT_ID();
  meta = response.meta();
}
void asp::pho::RemoteProjectFile::UpdateIteration( int32 const& i ) {
  ProjectUpdateRequest request;
  request.set_project_id( m_project_id );
  request.set_iteration( i );
  ProjectUpdateReply response;
  m_ptk_service->ProjectUpdate( m_rpc_controller.get(), &request, &response,
                                google::protobuf::NewCallback(&null_closure) );
  CHECK_PROJECT_ID();
}
int32 asp::pho::RemoteProjectFile::CreateCameraMeta( CameraMeta const& meta ) {
  CameraCreateRequest request;
  request.set_project_id( m_project_id );
  *(request.mutable_meta()) = meta;
  CameraCreateReply response;
  m_ptk_service->CameraCreate( m_rpc_controller.get(), &request, &response,
                               google::protobuf::NewCallback(&null_closure) );
  CHECK_PROJECT_ID();
  return response.camera_id();
}
void asp::pho::RemoteProjectFile::ReadCameraMeta( int32 i, CameraMeta& meta ) {
  CameraReadRequest request;
  request.set_project_id( m_project_id );
  request.set_camera_id( i );
  CameraReadReply response;
  m_ptk_service->CameraRead( m_rpc_controller.get(), &request, &response,
                             google::protobuf::NewCallback(&null_closure) );
  CHECK_PROJECT_ID();
  CHECK_CAMERA_ID(i);
  meta = response.meta();
}
void asp::pho::RemoteProjectFile::WriteCameraMeta( int32 i, CameraMeta const& meta ) {
  CameraWriteRequest request;
  request.set_project_id( m_project_id );
  request.set_camera_id( i );
  *(request.mutable_meta()) = meta;
  CameraWriteReply response;
  m_ptk_service->CameraWrite( m_rpc_controller.get(), &request, &response,
                              google::protobuf::NewCallback(&null_closure) );
  CHECK_PROJECT_ID();
  CHECK_CAMERA_ID(i);
}
