#ifndef __SHADERSTATE_H__
#define __SHADERSTATE_H__

#include <string>
#include <vector>

#include "RenderParameters.h"
#include "EffectParameters.h"
#include "AttributeLocation.h"

typedef int static_branch;

struct ShaderState
{
	virtual void HandleShaderFlags (std::vector<std::string> shaderFlags) = 0;
	virtual void CalculateShaderState (const RenderParameters& renderParameters, const EffectParameters& effectParameters) = 0;
	virtual void SetAttributeLocation (const AttributeLocation& attributeLocation) { m_attributeLocation = attributeLocation; }

	// Texture flags
	static_branch b_useDiffuseTexture;
	static_branch b_useEnvironmentMap;
	static_branch b_useNormalMap;
	static_branch b_source0;
	static_branch b_source1;

	// Buffer flags
	AttributeLocation m_attributeLocation;
};

#endif