/**
 * @file SdfGenerationOpenCL.cpp
 * @brief OpenCL 1.2 buffer-based SDF generation.
 */

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>

#include "SdfGenerationOpenCL.hpp"
#include "RGBA8888.hpp"

#include <QFile>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

/**
 * @brief Release an OpenCL buffer on scope exit.
 */
struct CLBuffer {
	cl_mem mem = nullptr;
	CLBuffer() = default;
	CLBuffer(const CLBuffer&) = delete;
	CLBuffer& operator=(const CLBuffer&) = delete;
	CLBuffer(CLBuffer&& other) noexcept : mem(other.mem) { other.mem = nullptr; }
	CLBuffer& operator=(CLBuffer&& other) noexcept { if(this != &other) { if(mem) clReleaseMemObject(mem); mem = other.mem; other.mem = nullptr; } return *this; }
	~CLBuffer()
	{
		if(mem) clReleaseMemObject(mem);
	}
};

/**
 * @brief Throw a readable exception for an OpenCL error code.
 */
void checkCl(cl_int error, const char* operation)
{
	if(error == CL_SUCCESS) return;
	throw std::runtime_error(QStringLiteral("OpenCL error %1 in %2").arg(error).arg(QString::fromLatin1(operation)).toStdString());
}

float gammaAdjust(float x)
{
	const float d = x - 0.5f;
	return 0.5f + 2.0f * d * d * d + 0.5f * d;
}

int sampleHalfWidth(const SDFGenerationArguments& args)
{
	return args.samples_to_check_x ? static_cast<int>(args.samples_to_check_x / 2) : static_cast<int>(args.padding);
}

int sampleHalfHeight(const SDFGenerationArguments& args)
{
	return args.samples_to_check_y ? static_cast<int>(args.samples_to_check_y / 2) : static_cast<int>(args.padding);
}

QImage finalizeSdf(const std::vector<float>& rawDistances,
				   const std::vector<unsigned int>& insideMask,
				   uint32_t size,
				   const SDFGenerationArguments& args)
{
	float maxDistIn = std::numeric_limits<float>::epsilon();
	float maxDistOut = std::numeric_limits<float>::epsilon();
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		if(insideMask[i]) maxDistIn = std::max(maxDistIn, std::abs(rawDistances[i]));
		else maxDistOut = std::max(maxDistOut, std::abs(rawDistances[i]));
	}

	std::vector<float> normalized = rawDistances;
	for(size_t i = 0; i < normalized.size(); ++i) {
		if(insideMask[i]) {
			normalized[i] /= maxDistIn;
			normalized[i] = 0.5f + normalized[i] * 0.5f;
		} else {
			normalized[i] /= maxDistOut;
			normalized[i] = 0.5f - normalized[i] * 0.5f;
		}
		if(args.invert) normalized[i] = 1.0f - normalized[i];
	}
	if(args.midpointAdjustment.has_value()) {
		const float divisor = args.midpointAdjustment.value_or(1.0f);
		for(float& value : normalized) value = std::clamp(value / divisor, 0.0f, 1.0f);
	}
	if(args.gammaCorrect) {
		for(float& value : normalized) value = gammaAdjust(value);
	}

	QImage image(static_cast<int>(size), static_cast<int>(size), QImage::Format_Grayscale8);
	for(uint32_t y = 0; y < size; ++y) {
		uchar* row = image.scanLine(static_cast<int>(y));
		const size_t rowStart = static_cast<size_t>(y) * size;
		for(uint32_t x = 0; x < size; ++x) {
			row[x] = static_cast<uchar>(normalized[rowStart + x] * 255.0f);
		}
	}
	return image;
}

QImage finalizeMsdf(const std::vector<float>& rawChannels,
					uint32_t size,
					const SDFGenerationArguments& args)
{
	const size_t pixelCount = static_cast<size_t>(size) * size;
	std::vector<glm::fvec4> values(pixelCount);
	for(size_t i = 0; i < pixelCount; ++i) {
		values[i] = glm::fvec4(rawChannels[i * 4 + 0], rawChannels[i * 4 + 1],
   rawChannels[i * 4 + 2], rawChannels[i * 4 + 3]);
	}

	glm::fvec4 minDist(std::numeric_limits<float>::max());
	glm::fvec4 maxDist(std::numeric_limits<float>::lowest());
	for(const glm::fvec4& value : values) {
		for(int c = 0; c < 4; ++c) {
			maxDist[c] = std::max(maxDist[c], value[c]);
			minDist[c] = std::min(minDist[c], value[c]);
		}
	}

	for(glm::fvec4& value : values) {
		for(int c = 0; c < 4; ++c) {
			if(value[c] >= 0.0f) value[c] /= maxDist[c];
			else value[c] = (value[c] / minDist[c]) * -1.0f;
			value[c] = value[c] * 0.5f + 0.5f;
			if(args.invert) value[c] = 1.0f - value[c];
		}
	}
	if(args.midpointAdjustment.has_value()) {
		const float divisor = args.midpointAdjustment.value_or(1.0f);
		for(glm::fvec4& value : values) {
			for(int c = 0; c < 4; ++c) value[c] = std::clamp(value[c] / divisor, 0.0f, 1.0f);
		}
	}
	if(args.gammaCorrect) {
		for(glm::fvec4& value : values) {
			for(int c = 0; c < 4; ++c) value[c] = gammaAdjust(value[c]);
		}
	}
	if(args.type != MSDFA) {
		for(glm::fvec4& value : values) value.a = 1.0f;
	}

	QImage image(static_cast<int>(size), static_cast<int>(size), QImage::Format_RGBA8888);
	for(uint32_t y = 0; y < size; ++y) {
		RGBA8888* row = reinterpret_cast<RGBA8888*>(image.scanLine(static_cast<int>(y)));
		for(uint32_t x = 0; x < size; ++x) {
			row[x].fromFvec4(values[static_cast<size_t>(y) * size + x]);
		}
	}
	return image;
}

}

struct SdfGenerationOpenCL::Impl {
	cl_device_id device = nullptr;
	cl_context context = nullptr;
	cl_command_queue queue = nullptr;
	cl_kernel bitmapKernel = nullptr;
	cl_kernel outlineKernel = nullptr;
	cl_kernel msdfKernel = nullptr;

	~Impl() { release(); }

	void release()
	{
		if(bitmapKernel) { clReleaseKernel(bitmapKernel); bitmapKernel = nullptr; }
		if(outlineKernel) { clReleaseKernel(outlineKernel); outlineKernel = nullptr; }
		if(msdfKernel) { clReleaseKernel(msdfKernel); msdfKernel = nullptr; }
		if(queue) { clReleaseCommandQueue(queue); queue = nullptr; }
		if(context) { clReleaseContext(context); context = nullptr; }
	}

	void initialize(const SDFGenerationArguments& args)
	{
		cl_int error = CL_SUCCESS;
		cl_uint platformCount = 0;
		checkCl(clGetPlatformIDs(0, nullptr, &platformCount), "clGetPlatformIDs");
		if(platformCount == 0) throw std::runtime_error("No OpenCL platform is available.");

		std::vector<cl_platform_id> platforms(platformCount);
		checkCl(clGetPlatformIDs(platformCount, platforms.data(), nullptr), "clGetPlatformIDs");

		cl_platform_id platform = nullptr;
		for(cl_platform_id candidate : platforms) {
			cl_uint deviceCount = 0;
			if(clGetDeviceIDs(candidate, CL_DEVICE_TYPE_GPU, 1, &device, &deviceCount) == CL_SUCCESS && deviceCount > 0) {
				platform = candidate;
				break;
			}
		}
		if(!device) {
			for(cl_platform_id candidate : platforms) {
				cl_uint deviceCount = 0;
				if(clGetDeviceIDs(candidate, CL_DEVICE_TYPE_ALL, 1, &device, &deviceCount) == CL_SUCCESS && deviceCount > 0) {
					platform = candidate;
					break;
				}
			}
		}
		(void)platform;
		if(!device) throw std::runtime_error("No OpenCL device is available.");

		context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &error);
		checkCl(error, "clCreateContext");
		queue = clCreateCommandQueue(context, device, 0, &error);
		checkCl(error, "clCreateCommandQueue");

		QFile sourceFile(QStringLiteral(":/sdf_opencl.cl"));
		if(!sourceFile.open(QFile::ReadOnly)) {
			throw std::runtime_error("Failed to open the embedded OpenCL kernel source.");
		}
		const QByteArray source = sourceFile.readAll();
		const char* sourcePtr = source.constData();
		const size_t sourceLength = static_cast<size_t>(source.size());
		cl_program program = clCreateProgramWithSource(context, 1, &sourcePtr, &sourceLength, &error);
		checkCl(error, "clCreateProgramWithSource");

		QByteArray options = QByteArrayLiteral("-cl-fast-relaxed-math -cl-unsafe-math-optimizations -cl-no-signed-zeros");
		if(args.distType == DistanceType::Manhattan) options += QByteArrayLiteral(" -DUSE_MANHATTAN_DISTANCE");
		error = clBuildProgram(program, 1, &device, options.constData(), nullptr, nullptr);
		if(error != CL_SUCCESS) {
			size_t logLength = 0;
			clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logLength);
			QByteArray log(static_cast<int>(logLength), '\0');
			if(logLength > 0) clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logLength, log.data(), nullptr);
			clReleaseProgram(program);
			throw std::runtime_error(QStringLiteral("OpenCL build failed:\n%1").arg(QString::fromUtf8(log)).toStdString());
		}

		bitmapKernel = clCreateKernel(program, "sdf_bitmap", &error);
		checkCl(error, "clCreateKernel(sdf_bitmap)");
		outlineKernel = clCreateKernel(program, "sdf_outline", &error);
		checkCl(error, "clCreateKernel(sdf_outline)");
		msdfKernel = clCreateKernel(program, "msdf_outline", &error);
		checkCl(error, "clCreateKernel(msdf_outline)");
		clReleaseProgram(program);
	}

	CLBuffer createBuffer(cl_mem_flags flags, size_t size, const void* host = nullptr)
	{
		cl_int error = CL_SUCCESS;
		CLBuffer buffer;
		buffer.mem = clCreateBuffer(context, flags | (host ? CL_MEM_COPY_HOST_PTR : 0), size,
const_cast<void*>(host), &error);
		checkCl(error, "clCreateBuffer");
		return buffer;
	}

	void runKernel(cl_kernel kernel, uint32_t size)
	{
		const size_t local[2] = {8, 8};
		const size_t global[2] = {static_cast<size_t>((size + 7u) / 8u * 8u),
								  static_cast<size_t>((size + 7u) / 8u * 8u)};
		checkCl(clEnqueueNDRangeKernel(queue, kernel, 2, nullptr, global, local, 0, nullptr, nullptr),
"clEnqueueNDRangeKernel");
	}

	QImage produceBitmap(const QImage& source, const SDFGenerationArguments& args)
	{
		if(args.type != SDF) {
			throw std::runtime_error("The OpenCL raster path supports SDF only; use outline processing for MSDF/MSDFA.");
		}
		const uint32_t size = args.internalProcessSize;
		if(size == 0) throw std::runtime_error("Internal processing size must be greater than zero.");
		const QImage grayscale = source.format() == QImage::Format_Grayscale8
				? source : source.convertToFormat(QImage::Format_Grayscale8);
		if(grayscale.width() != static_cast<int>(size) || grayscale.height() != static_cast<int>(size)) {
			throw std::runtime_error("OpenCL bitmap input does not match the internal processing size.");
		}

		std::vector<uchar> input(static_cast<size_t>(size) * size);
		for(uint32_t y = 0; y < size; ++y) {
			std::memcpy(input.data() + static_cast<size_t>(y) * size, grayscale.constScanLine(static_cast<int>(y)), size);
		}

		CLBuffer fontBuffer = createBuffer(CL_MEM_READ_ONLY, input.size() * sizeof(uchar), input.data());
		CLBuffer rawBuffer = createBuffer(CL_MEM_WRITE_ONLY, static_cast<size_t>(size) * size * sizeof(cl_float));
		CLBuffer maskBuffer = createBuffer(CL_MEM_WRITE_ONLY, static_cast<size_t>(size) * size * sizeof(cl_uint));

		cl_int sampleWidth = sampleHalfWidth(args);
		cl_int sampleHeight = sampleHalfHeight(args);
		cl_int width = static_cast<cl_int>(size);
		cl_int height = static_cast<cl_int>(size);
		checkCl(clSetKernelArg(bitmapKernel, 0, sizeof(cl_mem), &fontBuffer.mem), "clSetKernelArg");
		checkCl(clSetKernelArg(bitmapKernel, 1, sizeof(cl_mem), &rawBuffer.mem), "clSetKernelArg");
		checkCl(clSetKernelArg(bitmapKernel, 2, sizeof(cl_mem), &maskBuffer.mem), "clSetKernelArg");
		checkCl(clSetKernelArg(bitmapKernel, 3, sizeof(cl_int), &width), "clSetKernelArg");
		checkCl(clSetKernelArg(bitmapKernel, 4, sizeof(cl_int), &height), "clSetKernelArg");
		checkCl(clSetKernelArg(bitmapKernel, 5, sizeof(cl_int), &sampleWidth), "clSetKernelArg");
		checkCl(clSetKernelArg(bitmapKernel, 6, sizeof(cl_int), &sampleHeight), "clSetKernelArg");
		runKernel(bitmapKernel, size);

		std::vector<float> raw(static_cast<size_t>(size) * size);
		std::vector<unsigned int> mask(raw.size());
		checkCl(clEnqueueReadBuffer(queue, rawBuffer.mem, CL_TRUE, 0, raw.size() * sizeof(float), raw.data(), 0, nullptr, nullptr), "clEnqueueReadBuffer");
		checkCl(clEnqueueReadBuffer(queue, maskBuffer.mem, CL_TRUE, 0, mask.size() * sizeof(unsigned int), mask.data(), 0, nullptr, nullptr), "clEnqueueReadBuffer");
		return finalizeSdf(raw, mask, size, args);
	}

	QImage produceOutline(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args)
	{
		static_assert(sizeof(EdgeSegment) == 48, "OpenCL EdgeSegment layout must remain 48 bytes");
		const uint32_t size = args.internalProcessSize;
		if(size == 0) throw std::runtime_error("Internal processing size must be greater than zero.");
		if(source.edges.empty()) throw std::runtime_error("OpenCL outline input contains no edges.");

		const size_t edgeCount = source.edges.size();
		CLBuffer edgeBuffer = createBuffer(CL_MEM_READ_ONLY, edgeCount * sizeof(EdgeSegment), source.edges.data());
		const size_t pixelCount = static_cast<size_t>(size) * size;
		const bool isMsdf = args.type != SDF;
		const size_t channelCount = isMsdf ? 4u : 1u;
		CLBuffer rawBuffer = createBuffer(CL_MEM_WRITE_ONLY, pixelCount * channelCount * sizeof(cl_float));
		CLBuffer maskBuffer = createBuffer(CL_MEM_WRITE_ONLY, pixelCount * channelCount * sizeof(cl_uint));

		cl_kernel kernel = isMsdf ? msdfKernel : outlineKernel;
		cl_uint clEdgeCount = static_cast<cl_uint>(edgeCount);
		cl_int width = static_cast<cl_int>(size);
		cl_int height = static_cast<cl_int>(size);
		cl_int sampleWidth = sampleHalfWidth(args);
		cl_int sampleHeight = sampleHalfHeight(args);
		checkCl(clSetKernelArg(kernel, 0, sizeof(cl_mem), &edgeBuffer.mem), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 1, sizeof(cl_uint), &clEdgeCount), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 2, sizeof(cl_mem), &rawBuffer.mem), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 3, sizeof(cl_mem), &maskBuffer.mem), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 4, sizeof(cl_int), &width), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 5, sizeof(cl_int), &height), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 6, sizeof(cl_int), &sampleWidth), "clSetKernelArg");
		checkCl(clSetKernelArg(kernel, 7, sizeof(cl_int), &sampleHeight), "clSetKernelArg");
		runKernel(kernel, size);

		std::vector<float> raw(pixelCount * channelCount);
		checkCl(clEnqueueReadBuffer(queue, rawBuffer.mem, CL_TRUE, 0, raw.size() * sizeof(float), raw.data(), 0, nullptr, nullptr), "clEnqueueReadBuffer");
		if(isMsdf) return finalizeMsdf(raw, size, args);

		std::vector<unsigned int> mask(pixelCount);
		checkCl(clEnqueueReadBuffer(queue, maskBuffer.mem, CL_TRUE, 0, mask.size() * sizeof(unsigned int), mask.data(), 0, nullptr, nullptr), "clEnqueueReadBuffer");
		return finalizeSdf(raw, mask, size, args);
	}
};

SdfGenerationOpenCL::SdfGenerationOpenCL(const SDFGenerationArguments& args)
	: impl(std::make_unique<Impl>())
{
	try {
		impl->initialize(args);
	} catch(...) {
		impl->release();
		throw;
	}
}

SdfGenerationOpenCL::~SdfGenerationOpenCL() = default;

QImage SdfGenerationOpenCL::produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args)
{
	return impl->produceBitmap(source, args);
}

QImage SdfGenerationOpenCL::produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args)
{
	return impl->produceOutline(source, args);
}
