#ifndef SDFGENERATIONCONTEXTSOFT_HPP
#define SDFGENERATIONCONTEXTSOFT_HPP

#include "SdfGenerationContext.hpp"

class SdfGenerationContextSoft : public SdfGenerationContext
{
public:
	SdfGenerationContextSoft();
	QImage produceSdf(const QImage& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONCONTEXTSOFT_HPP
