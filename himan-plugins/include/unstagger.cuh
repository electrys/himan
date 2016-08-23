/**
 * File:   unstagger.cuh
 * Author: tack
 *
 * Created on July 19, 2016
 *
 * List of extern functions compiled by nvcc for plugins (compiled by gcc)
 */

#ifndef UNSTAGGER_CUDA_H
#define UNSTAGGER_CUDA_H
#ifdef HAVE_CUDA
#include <utility>
#include <vector>

namespace himan
{
namespace plugin
{
namespace unstagger_cuda
{
// initialize interpolation matrices
void Init(size_t NX, size_t NY);

// unstagger
std::pair<std::vector<double>, std::vector<double>> Process(std::vector<double>& U_in, std::vector<double>& V_in);

}  // namespace unstagger_cuda
}  // namespace plugin
}  // namespace himan

#endif /* HAVE_CUDA */
#endif /* UNSTAGGER_CUDA_H */
