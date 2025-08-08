#include <cuco/static_set.cuh>

#ifdef USE_CUDA
__global__
#endif
void HelloWorld() {

}


cuco::static_set<int> Myfunction() {
    HelloWorld<<<1, 1>>>();
}
