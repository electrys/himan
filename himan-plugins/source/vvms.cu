#include "cuda_plugin_helper.h"

using namespace himan;

template <typename Type>
__device__ Type VVMS(Type VV, Type T, Type P)
{
	ASSERT(P < 1200 || IsMissing(P));
	return 287 * -VV * T / (static_cast<Type>(himan::constants::kG) * 100 * P);
}

template <typename T>
__global__ void VVMSKernel(const T* __restrict__ d_t, const T* __restrict__ d_vv, const T* __restrict__ d_p,
                           T* __restrict__ d_vv_ms, T vv_scale, size_t N)
{
	const int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < N)
	{
		d_vv_ms[idx] = vv_scale * VVMS<T>(d_vv[idx], d_t[idx], d_p[idx]);
	}
}

template <typename T>
__global__ void VVMSKernel(const T* __restrict__ d_t, const T* __restrict__ d_vv, const T P, T* __restrict__ d_vv_ms,
                           T vv_scale, size_t N)
{
	const int idx = blockIdx.x * blockDim.x + threadIdx.x;

	if (idx < N)
	{
		d_vv_ms[idx] = vv_scale * VVMS<T>(d_vv[idx], d_t[idx], P);
	}
}

void ProcessGPU(std::shared_ptr<const plugin_configuration> conf, std::shared_ptr<info> myTargetInfo)
{
	cudaStream_t stream;
	CUDA_CHECK(cudaStreamCreate(&stream));

	// Allocate device arrays

	float* d_t = 0;
	float* d_p = 0;
	float* d_vv = 0;
	float* d_vv_ms = 0;

	const size_t N = myTargetInfo->SizeLocations();
	const size_t memsize = N * sizeof(float);

	CUDA_CHECK(cudaMalloc((void**)&d_vv_ms, memsize));
	CUDA_CHECK(cudaMalloc((void**)&d_t, memsize));
	CUDA_CHECK(cudaMalloc((void**)&d_vv, memsize));

	auto TInfo =
	    cuda::Fetch(conf, myTargetInfo->Time(), myTargetInfo->Level(), param("T-K"), myTargetInfo->ForecastType());
	auto VVInfo =
	    cuda::Fetch(conf, myTargetInfo->Time(), myTargetInfo->Level(), param("VV-PAS"), myTargetInfo->ForecastType());

	if (!TInfo || !VVInfo)
	{
		return;
	}

	cuda::PrepareInfo(TInfo, d_t, stream);
	cuda::PrepareInfo(VVInfo, d_vv, stream);

	// dims

	const int blockSize = 512;
	const int gridSize = N / blockSize + (N % blockSize == 0 ? 0 : 1);

	bool isPressureLevel = (myTargetInfo->Level().Type() == kPressure);

	const float vv_scale = (myTargetInfo->Param().Name() == "VV-MMS") ? 1000. : 1.;

	// "SetAB"

	if (myTargetInfo->Level().Type() == kHybrid)
	{
		const size_t paramIndex = myTargetInfo->ParamIndex();

		for (myTargetInfo->ResetParam(); myTargetInfo->NextParam();)
		{
			myTargetInfo->Grid()->AB(TInfo->Grid()->AB());
		}

		myTargetInfo->ParamIndex(paramIndex);
	}

	if (isPressureLevel == false)
	{
		CUDA_CHECK(cudaMalloc((void**)&d_p, memsize));

		auto PInfo = cuda::Fetch(conf, myTargetInfo->Time(), myTargetInfo->Level(), param("P-HPA"),
		                         myTargetInfo->ForecastType());
		cuda::PrepareInfo(PInfo, d_p, stream);

		VVMSKernel<float><<<gridSize, blockSize, 0, stream>>>(d_t, d_vv, d_p, d_vv_ms, vv_scale, N);
	}
	else
	{
		VVMSKernel<float><<<gridSize, blockSize, 0, stream>>>(d_t, d_vv, myTargetInfo->Level().Value(), d_vv_ms,
		                                                      vv_scale, N);
	}

	cuda::ReleaseInfo(myTargetInfo, d_vv_ms, stream);

	CUDA_CHECK(cudaStreamSynchronize(stream));

	CUDA_CHECK(cudaFree(d_t));
	CUDA_CHECK(cudaFree(d_vv));
	CUDA_CHECK(cudaFree(d_vv_ms));

	if (d_p)
	{
		// himan::ReleaseInfo(opts.p);
		CUDA_CHECK(cudaFree(d_p));
	}

	CUDA_CHECK(cudaStreamDestroy(stream));
}
