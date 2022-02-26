
// obj_opt.cpp
// This module optimizes an object loaded with tiny_obj_loader and stores it in a binary format for fast loading.
// This is essentially the "compiler" for OBJ files - or any file that can be read with tiny_obj_loader.
// Include this in your project, build it using a custom build step, and then use it to compile your OBJs into
//  optimized binary format which can be loaded very quickly at runtime.

// We must have a manner for an application to inject its Vertex implemnentation into the compilation - perhaps multiple
// different versions of a Vertex - depending on model type and usage. As such this Vertex impl should be included before this
// file is included in each "obj compiler executable" and the Vertex should be typdef'd vtyVertexType which is the type
// that will be used for each compilation using that vertex type.

// main() is defined in this modoule and thus each "vertex compiler executable" that is defined by a project needn't have a main().

__BIENUTIL_USING_NAMESPACE

string g_strProgramName;

int _TryMain( int _argc, char ** _argv );

int main( int _argc, char ** _argv )
{
#define USAGE "Usage: %s <input OBJ file> <output binary obj file>"
  g_strProgramName = _argv[0];
  n_SysLog::InitSysLog( g_strProgramName.c_str(), LOG_PERROR, LOG_USER );

  if ( 3 != _argc )
  {
    LOGSYSLOG( eslmtError, USAGE, g_strProgramName.c_str() );
    return EXIT_FAILURE;
  }

  try
  {
    return _TryMain( _argc-1, _argv + 1 );
  }
  catch( std::exception const & _rexc )
  {
    LOGEXCEPTION( _rexc, "Caught exception attempting to compile OBJ file." );
    return EXIT_FAILURE;
  }
  catch( ... )
  {
    LOGSYSLOG( eslmtError, "Unknown exception caught." );
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int _TryMain( int _argc, char ** _argv )
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string szWarn, szErr;
  VerifyThrowSz( tinyobj::LoadObj( &attrib, &shapes, &materials, &szWarn, &szErr, _argv[0] ), "Error loading model from [%s]. szWarn[%s], szErr[%s].", _argv[0], &szWarn[0], &szErr[0] );

  typedef ObjOptimizer< vtyVertexType > _TyObjOptimizer;
  _TyObjOptimizer objopt;
  try
  {
    objopt.OptimizeTinyObjShapes( attrib, shapes.data(), shapes.data() + shapes.size(), _argv[1] );
  }
  catch( std::exception const & _rexc )
  {
    LOGEXCEPTION( _rexc, "Caught exception attempting to compile OBJ file from [%s] into [%s].", _argv[0], _argv[1] );
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
