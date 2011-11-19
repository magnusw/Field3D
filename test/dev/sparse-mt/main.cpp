#include <Field3D/InitIO.h>
#include <Field3D/SparseFieldIO.h>
#include <Field3D/FieldInterp.h>

#include <pthread.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

double rand01()
{
  return rand() / (double)RAND_MAX;
}

void* sampleField( void *data )
{
  Field3D::Field<float>* field = ( Field3D::Field<float>* )data;
  Imath::V3i res = field->dataResolution();
  std::cout << "Reader thread: sampling field" << std::endl;

  float sum = 0;
  Field3D::LinearFieldInterp<float> linearFloatInterpolator;
  for( int i = 0; i < 10000000; i++ )
  {
    Imath::V3f pos((float)(rand01() * res.x), (float)(rand01() * res.y), (float)(rand01() * res.z));
    sum += linearFloatInterpolator.sample(*field, pos);
  }
  return NULL;

}

int main(int argc, char* argv[])
{    
  if ( argc != 2 )
  {
    std::cout << "Please specify an input sparse f3d file" << std::endl;;
    return 1;
  }

  std::cout << "F3d memory limit enabled" << std::endl;

  Field3D::initIO();
  Field3D::SparseFileManager &sparseManager = Field3D::SparseFileManager::singleton();
  sparseManager.setMaxMemUse(2000);
  sparseManager.setLimitMemUse(true);

  std::cout << "Loading sparse field from main thread" << std::endl;

  const char* f3dFile = argv[1];
  Field3D::Field3DInputFile in;
  in.open( f3dFile );
  Field3D::Field<float>::Vec fields = in.readScalarLayers<float>();
  std::cout << "Fields in file: " << fields.size() << std::endl;

  pthread_t readerThread;
  std::cout << "start reader thread" << std::endl;
  pthread_create( &readerThread, NULL, sampleField, (void*)( &*fields[ 0 ] ) );

  sleep(2); // ensure reader thread is working

  // do it many times to ensure the conflict between the reader thread and the main thread.

  std::cout << "opening cache again from main thread" << std::endl;

  for( int i = 0; i < 10; i++ ){
    // The bug usually manifests as boost assertion on the array destructor of the Reference<>::blockMutex
    // member variable. The reference is still being used by the reader thread, and the destructor
    // seems to be kicked off by STL reallocating the file references array on the sparse file manager when we
    // opened a new file from the main thread.

    Field3D::Field3DInputFile in2;
    in2.open( f3dFile );
    Field3D::Field<float>::Vec fields2 = in2.readScalarLayers<float>();
    std::cout << i << std::endl;
    sleep(1);
  }

  pthread_join(readerThread, 0);
  pthread_exit( 0 );

  return 0;
}
