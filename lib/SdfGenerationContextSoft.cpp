#include "SdfGenerationContextSoft.hpp"
#include <glm/glm.hpp>
#include <QBitArray>

SdfGenerationContextSoft::SdfGenerationContextSoft() {}

template <typename T> struct aligned_arr {
private:
	aligned_arr(const aligned_arr& cpy) = delete;
	aligned_arr& operator=(const aligned_arr& cpy) = delete;
public:
	T* ptr;
	aligned_arr( size_t alignment, size_t size ) {
		ptr = static_cast<T*>(aligned_alloc(alignment, size * sizeof(T)));
	}
	~aligned_arr() {
		if(ptr) free(ptr);
	}
	aligned_arr(aligned_arr&& mov) {
		this->ptr = mov.ptr;
		mov.ptr = nullptr;
	}
	aligned_arr& operator=(aligned_arr&& mov) {
		this->ptr = mov.ptr;
		mov.ptr = nullptr;
		return *this;
	}
};

struct TmpStoredDist {
	float f;
	bool isInside;
};

QImage SdfGenerationContextSoft::produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args)
{
	const auto width = args.internalProcessSize;
	const auto height = args.internalProcessSize;
	QImage sdf(width,height,QImage::Format_Grayscale8);
	QBitArray bitArr(width*height,false);
	for(unsigned y = 0; y < height; ++y) {
		const uint8_t* iRowStart = static_cast<const uint8_t*>( source.scanLine(y) );
		const size_t oRowStart = y*width;
		for(unsigned x = 0; x < width; ++x) {
			bitArr.setBit(oRowStart+x,iRowStart[x] >= 127 );
		}
	}

	//std::vector<float> tmpFloat(with * height);
	//double maxDist = sqrt(static_cast<double>(source.width()) * static_cast<double>(source.height()));
	const unsigned half_samples_to_check_x = args.samples_to_check_x ? args.samples_to_check_x / 2 : args.padding;
	const unsigned half_samples_to_check_y = args.samples_to_check_y ? args.samples_to_check_y / 2 : args.padding;

	const float maxDist = (args.distType == DistanceType::Euclidean)
							  ? (std::sqrt(static_cast<float>(half_samples_to_check_x) * static_cast<float>(half_samples_to_check_y)))
							  : static_cast<float>(half_samples_to_check_x+half_samples_to_check_y);

	const auto distanceCalculator = (args.distType == DistanceType::Euclidean)
										?
										( [](const glm::ivec2& a, const glm::ivec2& b) {
											return glm::distance(glm::fvec2(a),glm::fvec2(b));
										} )
										:
										( [](const glm::ivec2& a, const glm::ivec2& b) {
											return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
										} );

	std::vector<TmpStoredDist> storedDists(width * height);

	auto calculateSdfForPixel = [&bitArr,&distanceCalculator,maxDist,half_samples_to_check_x,half_samples_to_check_y,width,height](unsigned x, unsigned y, bool isInside) {
		const unsigned min_offset_x = static_cast<unsigned>(std::max( static_cast<int>(x)-static_cast<int>(half_samples_to_check_x), 0 ));
		const unsigned max_offset_x = static_cast<unsigned>(std::min( static_cast<int>(x)+static_cast<int>(half_samples_to_check_x), static_cast<int>(width) ));
		const unsigned min_offset_y = static_cast<unsigned>(std::max( static_cast<int>(y)-static_cast<int>(half_samples_to_check_y), 0 ));
		const unsigned max_offset_y = static_cast<unsigned>(std::min( static_cast<int>(y)+static_cast<int>(half_samples_to_check_y), static_cast<int>(height) ));
		float minDistance = maxDist;
		for(unsigned offset_y = min_offset_y; offset_y < max_offset_y; ++offset_y ) {
			const unsigned row_start = offset_y * width;
			for(unsigned offset_x = min_offset_x; offset_x < max_offset_x; ++offset_x ) {
				bool isEdge = bitArr.testBit(row_start+offset_x) != isInside;
				if(isEdge) {
					float dist = distanceCalculator(glm::ivec2(x,y),glm::ivec2(offset_x,offset_y));
					if(dist <= minDistance) minDistance = dist;
				}
			}
		}
		return minDistance;
	};
#pragma omp parallel for collapse(2)
	for(int y = 0; y < height;++y) {
		for(int x = 0; x < width; ++x) {
			TmpStoredDist* out_row_start = &storedDists[y * width];
			const unsigned in_row_start = y * width;
			const bool isInside = bitArr.testBit(in_row_start+x);
			out_row_start[x].isInside = isInside;
			out_row_start[x].f = calculateSdfForPixel(x,y,isInside);
		}
	}

	float maxDistIn = std::numeric_limits<float>::epsilon();
	float maxDistOut = std::numeric_limits<float>::epsilon();
	for(const auto& it : storedDists) {
		if(it.isInside) {
			maxDistIn = std::max(maxDistIn,it.f);
		} else {
			maxDistOut = std::max(maxDistOut,it.f);
		}
	}
	// Add a small epsilon to avoid division by zero
	for(auto& it : storedDists) {
		if(it.isInside) {
			it.f /= maxDistIn;
			it.f = 0.5f + (it.f * 0.5f);
		} else {
			it.f /= maxDistOut;
			it.f = 0.5f - (it.f * 0.5f);
		}
	}

	for(int y = 0; y < height;++y) {
		uchar* row = sdf.scanLine(y);
		const TmpStoredDist* in_row_start = &storedDists[y * width];
		for(int x = 0; x < width; ++x) {
			const TmpStoredDist& in = in_row_start[x];
			//float sdfValue = (in.isInside ? 0.5f + (in.f / 2.0f) : 0.5f - (in.f / 2.0f));
			row[x] = static_cast<uint8_t>(in.f * 255.0f);
		}
	}
	return sdf;
}

QImage SdfGenerationContextSoft::produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args)
{

}
