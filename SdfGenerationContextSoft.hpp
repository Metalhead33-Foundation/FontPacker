#ifndef SDFGENERATIONCONTEXTSOFT_HPP
#define SDFGENERATIONCONTEXTSOFT_HPP

#include "SdfGenerationContext.hpp"

class SdfGenerationContextSoft : public SdfGenerationContext
{
public:
	SdfGenerationContextSoft();
	QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) override;
	QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONCONTEXTSOFT_HPP
